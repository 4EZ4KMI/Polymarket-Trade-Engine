#include "portfolio/pt_portfolio.h"
#include <string.h>

void pt_portfolio_init(pt_portfolio_t *p, double initial_cash)
{
    memset(p, 0, sizeof(*p));
    p->initial_cash = initial_cash;
    p->cash         = initial_cash;
}

static pt_position_t *find_pos_(pt_portfolio_t *p, pt_market_id_t m, int is_yes)
{
    for (int i = 0; i < p->position_count; i++) {
        if (p->positions[i].market_id == m && p->positions[i].is_yes == is_yes)
            return &p->positions[i];
    }
    if (p->position_count < PT_PORTFOLIO_MAX_POSITIONS) {
        pt_position_t *pos = &p->positions[p->position_count++];
        memset(pos, 0, sizeof(*pos));
        pos->market_id = m;
        pos->is_yes = is_yes;
        return pos;
    }
    return NULL;
}

void pt_portfolio_on_fill(pt_portfolio_t *p, pt_market_id_t market_id,
                          int is_yes, int side, pt_size_t shares,
                          pt_price_t price_scaled)
{
    if (p == NULL || shares == 0) return;
    pt_position_t *pos = find_pos_(p, market_id, is_yes);
    if (!pos) return;

    double cost_usd = (double)shares * (double)price_scaled / (double)PT_PRICE_SCALE;

    if (side == PT_SIDE_BID) { /* buying */
        p->cash -= cost_usd;
        pos->shares += shares;
        pos->cost_basis_scaled += (int64_t)shares * (int64_t)price_scaled;
        p->total_exposure += cost_usd;
    } else { /* selling */
        p->cash += cost_usd;
        if (pos->shares > 0) {
            double avg_cost_per_share = (double)pos->cost_basis_scaled /
                                        (double)pos->shares / (double)PT_PRICE_SCALE;
            pt_size_t closed = (shares <= pos->shares) ? shares : pos->shares;
            double pnl = ((double)price_scaled / (double)PT_PRICE_SCALE - avg_cost_per_share) *
                         (double)closed;
            pos->realized_pnl += pnl;
            p->realized_pnl += pnl;
            p->trades_count++;
            if (pnl > 0.0) p->wins_count++;
            else if (pnl < 0.0) p->losses_count++;

            pos->cost_basis_scaled -= (int64_t)((double)pos->cost_basis_scaled *
                                                ((double)closed / (double)pos->shares));
            pos->shares -= closed;
            p->total_exposure -= avg_cost_per_share * (double)closed;
            if (p->total_exposure < 0.0) p->total_exposure = 0.0;
        }
    }
}

void pt_portfolio_mark(pt_portfolio_t *p, pt_market_id_t market_id,
                       int is_yes, pt_price_t current_bid)
{
    if (p == NULL) return;
    double unrl_sum = 0.0;
    for (int i = 0; i < p->position_count; i++) {
        pt_position_t *pos = &p->positions[i];
        if (pos->market_id == market_id && pos->is_yes == is_yes)
            pos->current_bid_price = current_bid;
        if (pos->shares > 0) {
            double avg_cost = (double)pos->cost_basis_scaled / (double)pos->shares /
                              (double)PT_PRICE_SCALE;
            double cur_val  = (double)pos->current_bid_price / (double)PT_PRICE_SCALE;
            pos->unrealized_pnl = (cur_val - avg_cost) * (double)pos->shares;
            unrl_sum += pos->unrealized_pnl;
        } else {
            pos->unrealized_pnl = 0.0;
        }
    }
    p->unrealized_pnl = unrl_sum;
}

double pt_portfolio_equity(const pt_portfolio_t *p)
{
    if (!p) return 0.0;
    return p->cash + p->total_exposure + p->unrealized_pnl;
}

double pt_portfolio_win_rate(const pt_portfolio_t *p)
{
    if (!p || p->trades_count == 0) return 0.0;
    return (double)p->wins_count / (double)p->trades_count;
}

double pt_portfolio_market_exposure(const pt_portfolio_t *p, pt_market_id_t market_id)
{
    if (!p) return 0.0;
    double exp = 0.0;
    for (int i = 0; i < p->position_count; i++) {
        if (p->positions[i].market_id == market_id && p->positions[i].shares > 0) {
            exp += (double)p->positions[i].cost_basis_scaled / (double)PT_PRICE_SCALE;
        }
    }
    return exp;
}
void pt_portfolio_settle_market(pt_portfolio_t *p, pt_market_id_t market_id, int winning_is_yes)
{
    if (!p) return;
    for (int i = 0; i < p->position_count; i++) {
        pt_position_t *pos = &p->positions[i];
        if (pos->market_id == market_id && pos->shares > 0) {
            double cost = (double)pos->cost_basis_scaled / (double)PT_PRICE_SCALE;
            double payoff = (pos->is_yes == winning_is_yes) ? (double)pos->shares * 1.0 : 0.0;
            double pnl = payoff - cost;

            p->cash += payoff;
            p->total_exposure -= cost;
            if (p->total_exposure < 0.0) p->total_exposure = 0.0;
            p->realized_pnl += pnl;
            p->trades_count++;

            if (pnl >= 0.0) p->wins_count++;
            else p->losses_count++;

            pos->shares = 0;
            pos->cost_basis_scaled = 0;
            pos->unrealized_pnl = 0.0;
            pos->realized_pnl += pnl;
        }
    }
}
