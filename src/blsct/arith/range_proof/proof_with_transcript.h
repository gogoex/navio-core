// Copyright (c) 2022 The Navcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NAVCOIN_BLSCT_ARITH_RANGE_PROOF_PROOF_WITH_TRANSCRIPT_H
#define NAVCOIN_BLSCT_ARITH_RANGE_PROOF_PROOF_WITH_TRANSCRIPT_H

#include <blsct/arith/elements.h>
#include <blsct/arith/scalar.h>
#include <blsct/arith/range_proof/proof.h>

class ProofWithTranscript
{
public:
    ProofWithTranscript(
        const Proof& proof,
        const Scalar& x,
        const Scalar& y,
        const Scalar& z,
        const Scalar& cx_factor,
        const Scalars& xs,
        const Scalars& inv_xs,
        const size_t& num_input_values_power_2,
        const size_t& concat_input_values_in_bits,
        const size_t& num_rounds
    ): proof{proof}, x{x}, y{y}, z{z}, cx_factor{cx_factor}, xs(xs),
        inv_xs{inv_xs}, inv_y(y.Invert()),
        num_input_values_power_2(num_input_values_power_2),
        concat_input_values_in_bits(concat_input_values_in_bits),
        num_rounds(num_rounds) {}

    static ProofWithTranscript Build(
      const Proof& proof,
      const size_t& num_rounds
    );

    const Proof proof;

    // transcript
    const Scalar x;  // x used in the main prove procedure
    const Scalar y;
    const Scalar z;
    const Scalar cx_factor;  // factor multiplied to cL and cR in inner product argument
    const Scalars xs;      // x used in inner product argument
    const Scalars inv_xs;  // x^-1 used in inner product argument
    const Scalar inv_y;

    const size_t num_input_values_power_2;  // M in old impl
    const size_t concat_input_values_in_bits;  // MN is old impl
    const size_t num_rounds;  // log2 M + log2 N in old impl
};

#endif // NAVCOIN_BLSCT_ARITH_RANGE_PROOF_PROOF_WITH_TRANSCRIPT_H