// Copyright (c) 2021-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RECEIVE_H
#define BITCOIN_WALLET_RECEIVE_H

#include <consensus/amount.h>
#include <wallet/transaction.h>
#include <wallet/types.h>
#include <wallet/wallet.h>

namespace wallet {
isminetype InputIsMine(const CWallet& wallet, const CTxIn& txin) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

/** Returns whether all of the inputs match the filter */
bool AllInputsMine(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter);

CAmount OutputGetCredit(const CWallet& wallet, const CTxOut& txout, const isminefilter& filter, const TokenId& token_id = TokenId());
CAmount OutputGetCredit(const CWallet& wallet, const CWalletOutput& wout, const isminefilter& filter, const TokenId& token_id, bool fIgnoreImmature = true);
CAmount TxGetCredit(const CWallet& wallet, const CTransaction& tx, const isminefilter& filter, const TokenId& token_id = TokenId());

bool ScriptIsChange(const CWallet& wallet, const CScript& script) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
bool OutputIsChange(const CWallet& wallet, const CTxOut& txout, const TokenId& token_id = TokenId()) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount OutputGetChange(const CWallet& wallet, const CTxOut& txout, const TokenId& token_id = TokenId()) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount TxGetChange(const CWallet& wallet, const CTransaction& tx, const TokenId& token_id = TokenId());

CAmount CachedTxGetCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter, const TokenId& token_id = TokenId())
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
//! filter decides which addresses will count towards the debit
CAmount CachedTxGetDebit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter, const TokenId& token_id = TokenId());
CAmount CachedTxGetChange(const CWallet& wallet, const CWalletTx& wtx, const TokenId& token_id = TokenId());
CAmount CachedTxGetImmatureCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter, const TokenId& token_id = TokenId())
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
CAmount CachedTxGetAvailableCredit(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter = ISMINE_SPENDABLE | ISMINE_SPENDABLE_BLSCT, const TokenId& token_id = TokenId())
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};
void CachedTxGetAmounts(const CWallet& wallet, const CWalletTx& wtx,
                        std::list<COutputEntry>& listReceived,
                        std::list<COutputEntry>& listSent,
                        CAmount& nFee, const isminefilter& filter,
                        bool include_change, const TokenId& token_id = TokenId());
bool CachedTxIsFromMe(const CWallet& wallet, const CWalletTx& wtx, const isminefilter& filter);
bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx, std::set<uint256>& trusted_parents) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
bool CachedTxIsTrusted(const CWallet& wallet, const CWalletTx& wtx);
bool IsOutputTrusted(const CWallet& wallet, const CWalletOutput& wout) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

struct Balance {
    CAmount m_mine_trusted{0};           //!< Trusted, at depth=GetBalance.min_depth or more
    CAmount m_mine_staked_commitment{0}; //!< Staked Commitment value
    CAmount m_mine_untrusted_pending{0}; //!< Untrusted, but in mempool (pending)
    CAmount m_mine_immature{0};          //!< Immature coinbases in the main chain
    CAmount m_watchonly_trusted{0};
    CAmount m_watchonly_untrusted_pending{0};
    CAmount m_watchonly_immature{0};
};
Balance GetBalance(const CWallet& wallet, int min_depth = 0, bool avoid_reuse = true, const TokenId& token_id = TokenId());
Balance GetBlsctBalance(const CWallet& wallet, int min_depth = 0, const TokenId& token_id = TokenId());

std::map<CTxDestination, CAmount> GetAddressBalances(const CWallet& wallet, const TokenId& token_id = TokenId());
std::set<std::set<CTxDestination>> GetAddressGroupings(const CWallet& wallet, const TokenId& token_id = TokenId()) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

struct StakedCommitmentInfo {
    Txid hashTx;
    size_t nout;
    MclG1Point commitment;
    MclScalar value;
    MclScalar gamma;
};

std::vector<StakedCommitmentInfo>
GetStakedCommitmentInfo(const CWallet& wallet, const CWalletTx& wtx) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
std::vector<StakedCommitmentInfo> GetStakedCommitmentInfo(const CWallet& wallet) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);
} // namespace wallet

#endif // BITCOIN_WALLET_RECEIVE_H
