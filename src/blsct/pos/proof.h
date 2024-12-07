// Copyright (c) 2024 The Navio Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLSCT_POS_PROOF_H
#define BLSCT_POS_PROOF_H

#include <arith_uint256.h>
#include <blsct/arith/mcl/mcl.h>
#include <blsct/pos/helpers.h>
#include <blsct/range_proof/bulletproofs_plus/range_proof.h>
#include <blsct/range_proof/bulletproofs_plus/range_proof_logic.h>
#include <blsct/set_mem_proof/set_mem_proof.h>
#include <blsct/set_mem_proof/set_mem_proof_prover.h>
#include <uint256.h>

using Arith = Mcl;
using Point = Arith::Point;
using Scalar = Arith::Scalar;
using Points = Elements<Point>;
using SetProof = SetMemProof<Arith>;
using RangeProof = bulletproofs_plus::RangeProof<Arith>;

namespace blsct {
class ProofOfStake
{
public:
    ProofOfStake()
    {
    }

    ProofOfStake(SetProof setMemProof, RangeProof rangeProof) : setMemProof(setMemProof), rangeProof(rangeProof)
    {
    }

    ProofOfStake(const Points& staked_commitments, const Scalar& eta_fiat_shamir, const blsct::Message& eta_phi, const Scalar& m, const Scalar& f, const uint32_t& prev_time, const uint64_t& stake_modifier, const uint32_t& time, const unsigned int& next_target);

    enum VerificationResult : uint32_t {
        NONE = 0,
        VALID = 1,
        RP_INVALID = 2,
        SM_INVALID = 3,
    };

    static std::string VerificationResultToString(const VerificationResult& res)
    {
        switch (res) {
        case VALID:
            return "Valid";
            break;
        case RP_INVALID:
            return "Invalid Range Proof";
            break;
        case SM_INVALID:
            return "Invalid Set Membership Proof";
            break;
        default:
            return "None";
        }
    }

    VerificationResult
    Verify(const Points& staked_commitments, const Scalar& eta_fiat_shamir, const blsct::Message& eta_phi, const uint256& kernelHash, const unsigned int& posTarget) const;
    VerificationResult Verify(const Points& staked_commitments, const Scalar& eta_fiat_shamir, const blsct::Message& eta_phi, const uint32_t& prev_time, const uint64_t& stake_modifier, const uint32_t& time, const unsigned int& next_target) const;

    static bool VerifyKernelHash(const RangeProof& range_proof, const uint256& kernel_hash, const unsigned int& next_target, const blsct::Message& eta_phi, const Point& phi);
    static bool VerifyKernelHash(const RangeProof& range_proof, const uint256& min_value, const blsct::Message& eta_phi, const Point& phi);

    static uint256 CalculateMinValue(const uint256& kernel_hash, const unsigned int& next_target);

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        ::Serialize(s, setMemProof);
        ::Serialize(s, rangeProof);
    }

    template <typename Stream>
    void Unserialize(Stream& s)
    {
        ::Unserialize(s, setMemProof);
        ::Unserialize(s, rangeProof);
    }

    SetProof setMemProof;
    RangeProof rangeProof;
};
} // namespace blsct

#endif // BLSCT_POS_PROOF_H