// Copyright (c) 2023 The Navio developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLSCT_VERIFICATION_H
#define BLSCT_VERIFICATION_H

#include <blsct/tokens/predicate_parser.h>
#include <chain.h>
#include <coins.h>
#include <consensus/validation.h>

namespace blsct {
bool VerifyTx(const CTransaction& tx, CCoinsViewCache& view, TxValidationState& state, const CAmount& blockReward = 0, const CAmount& minStake = 0);
}
#endif // BLSCT_VERIFICATION_H
