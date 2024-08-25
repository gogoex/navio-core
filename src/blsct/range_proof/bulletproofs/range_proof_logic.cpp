// Copyright (c) 2022 The Navio developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blsct/arith/mcl/mcl.h>
#include <blsct/arith/mcl/mcl_g1point.h>
#include <blsct/arith/mcl/mcl_scalar.h>
#include <blsct/building_block/fiat_shamir.h>
#include <blsct/building_block/g_h_gi_hi_zero_verifier.h>
#include <blsct/building_block/imp_inner_prod_arg.h>
#include <blsct/building_block/lazy_points.h>
#include <blsct/common.h>
#include <blsct/range_proof/bulletproofs/range_proof.h>
#include <blsct/range_proof/bulletproofs/range_proof_logic.h>
#include <blsct/range_proof/common.h>
#include <blsct/range_proof/msg_amt_cipher.h>
#include <future>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

namespace bulletproofs {

template <typename T>
RangeProof<T> RangeProofLogic<T>::Prove(
    Elements<typename T::Scalar> vs,
    const range_proof::GammaSeed<T>& nonce,
    const std::vector<uint8_t>& message,
    const Seed& seed,
    const typename T::Scalar& minValue) const
{
    using Scalar = typename T::Scalar;
    using Scalars = Elements<Scalar>;

    range_proof::Common<T>::ValidateParameters(vs, message);

    const size_t num_input_values_power_of_2 =
        blsct::Common::GetFirstPowerOf2GreaterOrEqTo(vs.Size());

    // this is power of 2 as well since m_input_value_bits is power of 2
    const size_t concat_input_values_in_bits =
        num_input_values_power_of_2 * range_proof::Setup::num_input_value_bits;

    ////////////// Proving steps
    RangeProof<T> proof;
    auto vsOriginal = vs;

    // apply minValue
    if (!minValue.IsZero()) {
        for (size_t i = 0; i < vs.Size(); ++i) {
            vs[i] = vs[i] - minValue;
        }
    }

    // generate gammas
    Scalars gammas;
    if (std::holds_alternative<Point>(nonce.seed)) {
        for (size_t i = 0; i < num_input_values_power_of_2; ++i) {
            auto hash = nonce.GetHashWithSalt(100 + i);
            gammas.Add(hash);
        }
    } else if (std::holds_alternative<Scalars>(nonce.seed)) {
        auto vec = std::get<Scalars>(nonce.seed);
        if (vs.Size() != vec.Size()) {
            throw std::runtime_error(strprintf("%s: size of vs does not match size of gammas", __func__));
        }
        for (size_t i = 0; i < vec.Size(); ++i) {
            gammas.Add(vec[i]);
        }
    }

    // make the number of input values a power of 2 w/ 0s if needed
    while (vs.Size() < num_input_values_power_of_2) {
        vs.Add(Scalar(0));
        vsOriginal.Add(Scalar(0));
    }

    // Get Generators<P> for the token_id
    range_proof::Generators<T> gens = m_common.Gf().GetInstance(seed);
    auto Gi = gens.GetGiSubset(concat_input_values_in_bits);
    auto Hi = gens.GetHiSubset(concat_input_values_in_bits);
    auto H = gens.H;
    auto G = gens.G;

    // This hash is updated for Fiat-Shamir throughout the proof
    HashWriter fiat_shamir{};

    // Calculate value commitments directly form the input values
    for (size_t i = 0; i < vs.Size(); ++i) {
        auto V = (G * vsOriginal[i]) + (H * gammas[i]);
        proof.Vs.Add(V);
        fiat_shamir << V;
    }

    // (41)-(42)
    // Values to be obfuscated are encoded in binary and flattened to a single vector aL
    // only the first 64 bits of each Scalar<S> is picked up
    Scalars aL;                  // ** size of aL can be shorter than concat_input_values_in_bits
    for (Scalar& v : vs.m_vec) { // for each input value
        for (size_t i = 0; i < range_proof::Setup::num_input_value_bits; ++i) {
            aL.Add(v.GetSeriBit(i) ? 1 : 0);
        }
    }
    // pad 0 bits at the end if aL.size < concat_input_values_in_bits
    while (aL.Size() < concat_input_values_in_bits) {
        aL.Add(0);
    }

    auto one_value_concat_bits = Scalars::RepeatN(m_common.One(), concat_input_values_in_bits);

    // aR is aL - 1
    Scalars aR = aL - one_value_concat_bits;

retry: // hasher is not cleared so that different hash will be obtained upon retry

    // (43)-(44)
    // Commitment to aL and aR (obfuscated with alpha)
    Scalar nonce_alpha = nonce.GetHashWithSalt(1);
    Scalar alpha = range_proof::MsgAmtCipher<T>::ComputeAlpha(message, vsOriginal[0], nonce_alpha);

    // Using generator H for alpha following the paper
    proof.A = (LazyPoints<T>(Gi, aL) + LazyPoints<T>(Hi, aR) + LazyPoint<T>(H, alpha)).Sum();

    // (45)-(47)
    // Commitment to blinding vectors sL and sR (obfuscated with rho)
    auto sL = Scalars::RandVec(concat_input_values_in_bits, true);
    auto sR = Scalars::RandVec(concat_input_values_in_bits, true);

    auto rho = nonce.GetHashWithSalt(2);

    // Using generator H for alpha following the paper
    proof.S = (LazyPoints<T>(Gi, sL) + LazyPoints<T>(Hi, sR) + LazyPoint<T>(H, rho)).Sum();

    // (48)-(50)
    fiat_shamir << proof.A;
    fiat_shamir << proof.S;

    GEN_FIAT_SHAMIR_VAR(y, fiat_shamir, retry);
    GEN_FIAT_SHAMIR_VAR(z, fiat_shamir, retry);

    // Polynomial construction by coefficients
    // AFTER (50)

    // l(x) = (aL - z 1^n) + sL X
    Scalars zs = Scalars::RepeatN(z, concat_input_values_in_bits);
    Scalars l0 = aL - zs;

    // l(1) is (aL - z 1^n) + sL, but this is reduced to sL
    const Scalars& l1 = sL;

    // Calculation of r(0) and r(1) on page 19
    Scalars z_pow_twos;
    Scalars z_pows_from_2 = Scalars::FirstNPow(z, num_input_values_power_of_2, 2); // z_pows excludes z^0 and z^1

    // The last term of r(X) on page 19
    for (size_t i = 0; i < num_input_values_power_of_2; ++i) {
        auto base_z_pow = z_pows_from_2[i]; // use different Scalar<S> for each input value

        for (size_t bit_idx = 0; bit_idx < range_proof::Setup::num_input_value_bits; ++bit_idx) {
            z_pow_twos.Add(base_z_pow * m_common.TwoPows64()[bit_idx]);
        }
    }

    Scalars y_pows = Scalars::FirstNPow(y, concat_input_values_in_bits);
    Scalars r0 = (y_pows * (aR + zs)) + z_pow_twos;
    Scalars r1 = y_pows * sR;

    // Polynomial construction before (51)
    Scalar t1 = (l0 * r1).Sum() + (l1 * r0).Sum();
    Scalar t2 = (l1 * r1).Sum();

    // (52)-(53)
    Scalar nonce_tau1 = nonce.GetHashWithSalt(3);
    Scalar tau2 = nonce.GetHashWithSalt(4);

    Scalar msg2 = range_proof::MsgAmtCipher<T>::RetrieveMsg2(message);
    Scalar tau1 = nonce_tau1 + msg2;

    proof.T1 = (G * t1) + (H * tau1);
    proof.T2 = (G * t2) + (H * tau2);

    // (54)-(56)
    fiat_shamir << proof.T1;
    fiat_shamir << proof.T2;

    GEN_FIAT_SHAMIR_VAR(x, fiat_shamir, retry);

    // (58)-(59)
    Scalars l = l0 + (l1 * x); // l0 = aL - z_mn; l1 = sL
    Scalars r = r0 + (r1 * x); // r0 = RHS of (58) - r1; r1 = y_mn o (sR * x)

    // LHS of (60)
    proof.t_hat = (l * r).Sum();

    // RHS of (60)
    Scalar t0 = (l0 * r0).Sum();
    Scalar t_of_x = t0 + t1 * x + t2 * x.Square();

    // (60)
    if (proof.t_hat != t_of_x)
        throw std::runtime_error(strprintf("%s: equality didn't hold in (60)", __func__));

    proof.tau_x = range_proof::MsgAmtCipher<T>::ComputeTauX(
        message,
        x,
        z,
        nonce_tau1,
        tau2,
        z_pows_from_2,
        gammas);
    proof.mu = alpha + (rho * x); // (62)

    // (63)
    fiat_shamir << proof.tau_x;
    fiat_shamir << proof.mu;
    fiat_shamir << proof.t_hat;

    GEN_FIAT_SHAMIR_VAR(c_factor, fiat_shamir, retry);

    {
        auto res = ImpInnerProdArg::Run<Mcl>(
            concat_input_values_in_bits,
            Gi, Hi, G,
            l, r,
            c_factor, y,
            fiat_shamir);
        if (res == std::nullopt) goto retry;

        proof.Ls = res.value().Ls;
        proof.Rs = res.value().Rs;
        proof.a = res.value().a;
        proof.b = res.value().b;
    }

    return proof;
}
template RangeProof<Mcl> RangeProofLogic<Mcl>::Prove(
    Elements<Mcl::Scalar>,
    const range_proof::GammaSeed<Mcl>&,
    const std::vector<uint8_t>&,
    const Seed&,
    const Mcl::Scalar&) const;

template <typename T>
bool RangeProofLogic<T>::VerifyProofs(
    const std::vector<RangeProofWithTranscript<T>>& proof_transcripts,
    const size_t& max_mn) const
{
    using Scalar = typename T::Scalar;
    using Scalars = Elements<Scalar>;

    // Vector to hold future results from async tasks
    std::vector<std::future<bool>> futures;

    // Launch a verification task for each proof transcript in parallel
    for (const RangeProofWithTranscript<T>& p : proof_transcripts) {
        futures.emplace_back(std::async(std::launch::async, [this, &p, max_mn]() -> bool {
            if (p.proof.Ls.Size() != p.proof.Rs.Size()) return false;

            const range_proof::Generators<T> gens = m_common.Gf().GetInstance(p.proof.seed);
            G_H_Gi_Hi_ZeroVerifier<T> verifier(max_mn);

            auto num_rounds = range_proof::Common<T>::GetNumRoundsExclLast(p.proof.Vs.Size());
            Scalar weight_y = Scalar::Rand();
            Scalar weight_z = Scalar::Rand();

            Scalars z_pows_from_2 = Scalars::FirstNPow(p.z, p.num_input_values_power_2 + 1, 2); // z^2, z^3, ...
            Scalar y_pows_sum = Scalars::FirstNPow(p.y, p.concat_input_values_in_bits).Sum();

            //////// (65)
            verifier.AddNegativeH(p.proof.tau_x * weight_y);

            Scalar delta_yz = p.z * y_pows_sum - (z_pows_from_2[0] * y_pows_sum);
            for (size_t i = 1; i <= p.num_input_values_power_2; ++i) {
                delta_yz = delta_yz - z_pows_from_2[i] * m_common.InnerProd1x2Pows64();
            }

            verifier.AddNegativeG((p.proof.t_hat - delta_yz) * weight_y);

            for (size_t i = 0; i < p.proof.Vs.Size(); ++i) {
                verifier.AddPoint(LazyPoint<T>(p.proof.Vs[i] - (gens.G * p.proof.min_value), z_pows_from_2[i] * weight_y));
            }

            verifier.AddPoint(LazyPoint<T>(p.proof.T1, p.x * weight_y));
            verifier.AddPoint(LazyPoint<T>(p.proof.T2, p.x.Square() * weight_y));

            //////// (66)
            verifier.AddPoint(LazyPoint<T>(p.proof.A, weight_z));
            verifier.AddPoint(LazyPoint<T>(p.proof.S, p.x * weight_z));

            //////// (67), (68)
            auto gen_exps = ImpInnerProdArg::GenGeneratorExponents<T>(num_rounds, p.xs);

            ImpInnerProdArg::LoopWithYPows<Mcl>(p.concat_input_values_in_bits, p.y,
                                                [&](const size_t& i, const Scalar& y_pow, const Scalar& y_inv_pow) {
                                                    Scalar gi_exp = p.proof.a * gen_exps[i];
                                                    Scalar hi_exp = p.proof.b * y_inv_pow * gen_exps[p.concat_input_values_in_bits - 1 - i];

                                                    gi_exp = gi_exp + p.z;

                                                    Scalar tmp = z_pows_from_2[i / range_proof::Setup::num_input_value_bits] *
                                                                 m_common.TwoPows64()[i % range_proof::Setup::num_input_value_bits];

                                                    hi_exp = hi_exp - (tmp + p.z * y_pow) * y_inv_pow;

                                                    verifier.SetGiExp(i, (gi_exp * weight_z).Negate());
                                                    verifier.SetHiExp(i, (hi_exp * weight_z).Negate());
                                                });

            verifier.AddNegativeH(p.proof.mu * weight_z);
            auto x_invs = p.xs.Invert();

            for (size_t i = 0; i < num_rounds; ++i) {
                verifier.AddPoint(LazyPoint<T>(p.proof.Ls[i], p.xs[i].Square() * weight_z));
                verifier.AddPoint(LazyPoint<T>(p.proof.Rs[i], x_invs[i].Square() * weight_z));
            }

            verifier.AddPositiveG((p.proof.t_hat - p.proof.a * p.proof.b) * p.c_factor * weight_z);

            bool res = verifier.Verify(
                gens.G,
                gens.H,
                gens.GetGiSubset(max_mn),
                gens.GetHiSubset(max_mn));
            return res;
        }));
    }

    // Wait for all threads to finish and collect results
    for (auto& fut : futures) {
        if (!fut.get()) return false;
    }

    return true;
}
template bool RangeProofLogic<Mcl>::VerifyProofs(
    const std::vector<RangeProofWithTranscript<Mcl>>&,
    const size_t&) const;

template <typename T>
bool RangeProofLogic<T>::Verify(
    const std::vector<RangeProofWithSeed<T>>& proofs) const
{
    range_proof::Common<T>::ValidateProofsBySizes(proofs);

    std::vector<RangeProofWithTranscript<T>> proof_transcripts;
    size_t max_num_rounds = 0;

    for (const RangeProofWithSeed<T>& proof : proofs) {
        // update max # of rounds and sum of all V bits
        max_num_rounds = std::max(max_num_rounds, proof.Ls.Size());

        // derive transcript from the proof
        auto proof_transcript = RangeProofWithTranscript<T>::Build(proof);
        proof_transcripts.push_back(proof_transcript);
    }

    const size_t max_mn = 1ull << max_num_rounds;

    return VerifyProofs(
        proof_transcripts,
        max_mn);
}
template bool RangeProofLogic<Mcl>::Verify(
    const std::vector<RangeProofWithSeed<Mcl>>&) const;

template <typename T>
AmountRecoveryResult<T> RangeProofLogic<T>::RecoverAmounts(
    const std::vector<AmountRecoveryRequest<T>>& reqs) const
{
    using Scalar = typename T::Scalar;
    using Point = typename T::Point;

    // will contain result of successful requests only
    std::vector<range_proof::RecoveredData<T>> xs;

    for (size_t i = 0; i < reqs.size(); ++i) {
        auto req = reqs[i];
        const range_proof::Generators<T> gens = m_common.Gf().GetInstance(req.seed);
        Point G = gens.G;
        Point H = gens.H;

        // failure if sizes of Ls and Rs differ or Vs is empty
        auto Ls_Rs_valid = req.Ls.Size() > 0 && req.Ls.Size() == req.Rs.Size();
        if (req.Vs.Size() == 0 || !Ls_Rs_valid) {
            continue;
        }
        // recovery can only be done when the number of value commitment is 1
        if (req.Vs.Size() != 1) {
            continue;
        }

        // mu is defined to be: mu = alpha + rho * x
        //
        // where alpha = nonce-alpha + (msg1 << 64 | 64-bit vs[0]). so,
        //
        // mu - rho * x = nonce-alpha + (msg1 << 64 | 64-bit vs[0])
        // mu - rho * x - nonce-alpha = (msg1 << 64 | 64-bit vs[0])
        // mu - rho * x - nonce-alpha = msg1_vs0
        //
        Scalar alpha = req.nonce.GetHashWithSalt(1);
        Scalar rho = req.nonce.GetHashWithSalt(2);
        Scalar tau1 = req.nonce.GetHashWithSalt(3);
        Scalar tau2 = req.nonce.GetHashWithSalt(4);
        Scalar gamma_vs0;

        // generate gammas
        if (std::holds_alternative<Point>(req.nonce.seed)) {
            gamma_vs0 = std::get<Point>(req.nonce.seed).GetHashWithSalt(100);
        } else if (std::holds_alternative<Scalars>(req.nonce.seed)) {
            gamma_vs0 = std::get<Scalars>(req.nonce.seed)[0];
        }

        Scalar msg1_vs0 = (req.mu - rho * req.x) - alpha;

        auto maybe_msg_amt = range_proof::MsgAmtCipher<T>::Decrypt(
            msg1_vs0,
            gamma_vs0,
            tau1,
            tau2,
            req.tau_x,
            req.x,
            req.z,
            m_common.Uint64Max(),
            H,
            G,
            req.Vs[0]);
        if (maybe_msg_amt == std::nullopt) {
            continue;
        }
        auto msg_amt = maybe_msg_amt.value();

        auto x = range_proof::RecoveredData<T>(
            req.id,
            msg_amt.amount,
            req.nonce.GetHashWithSalt(100), // gamma for vs[0]
            msg_amt.msg);

        xs.push_back(x);
    }
    return {
        true,
        xs};
}
template AmountRecoveryResult<Mcl> RangeProofLogic<Mcl>::RecoverAmounts(
    const std::vector<AmountRecoveryRequest<Mcl>>&) const;

} // namespace bulletproofs
