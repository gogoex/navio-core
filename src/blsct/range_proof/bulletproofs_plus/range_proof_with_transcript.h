// Copyright (c) 2023 The Navio developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NAVIO_BLSCT_ARITH_RANGE_PROOF_BULLETPROOFS_PLUS_RANGE_PROOF_WITH_TRANSCRIPT_H
#define NAVIO_BLSCT_ARITH_RANGE_PROOF_BULLETPROOFS_PLUS_RANGE_PROOF_WITH_TRANSCRIPT_H

#include <blsct/arith/elements.h>
#include <blsct/range_proof/proof_base.h>
#include <blsct/range_proof/bulletproofs_plus/range_proof.h>

namespace bulletproofs_plus {

template <typename T>
class RangeProofWithTranscript
{
    using Scalar = typename T::Scalar;
    using Point = typename T::Point;
    using Scalars = Elements<Scalar>;

public:
    RangeProofWithTranscript(
        const RangeProofWithSeed<T>& proof,
        const Scalar& y,
        const Scalar& z,
        const Scalar& e_last_round,
        const Scalars& es,
        const size_t& m,
        const size_t& n,
        const size_t& mn) : proof{proof}, y{y}, z{z}, e_last_round(e_last_round), es{es}, m{m}, n{n}, mn{mn} {}

    static RangeProofWithTranscript<T> Build(const RangeProofWithSeed<T>& proof);

    const RangeProofWithSeed<T> proof;
    const Scalar y;
    const Scalar z;
    const Scalar e_last_round;
    const Scalars es;
    const size_t m;
    const size_t n;
    const size_t mn;
};

} // namespace bulletproofs_plus

#endif // NAVIO_BLSCT_ARITH_RANGE_PROOF_BULLETPROOFS_PLUS_RANGE_PROOF_WITH_TRANSCRIPT_H
