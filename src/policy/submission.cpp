#include <utility>
#include <algorithm>
#include <vector>
#include <iostream>
#include "state.hpp"
#include "submission.hpp"

/*============================================================
 * Move Ordering Helper
 *============================================================*/
struct ScoredMove {
    Move move;
    int score;
};

static void order_moves(State* state, std::vector<Move>& moves) {
    if (moves.size() <= 1) return;

    std::vector<ScoredMove> scored_moves;
    scored_moves.reserve(moves.size());

    int opponent = 1 - state->player;
    for (const auto& m : moves) {
        int score = 0;
        int victim = state->piece_at(opponent, m.second.first, m.second.second);
        if (victim > 0) {
            score = 1000 + victim;
        }
        scored_moves.push_back({m, score});
    }

    std::sort(scored_moves.begin(), scored_moves.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    for (size_t i = 0; i < moves.size(); ++i) {
        moves[i] = scored_moves[i].move;
    }
}

/*============================================================
 * AlphaBetaExpert — quiescence
 *============================================================*/
int Submission::quiescence(
    State *state,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const SubmissionParams& p
) {
    ctx.nodes++;
    if (ctx.stop) return 0;

    int stand_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
        state->get_legal_actions();
    }

    std::vector<Move> captures;
    int opponent = 1 - state->player;
    for (const auto& m : state->legal_actions) {
        if (state->piece_at(opponent, m.second.first, m.second.second) != 0) {
            captures.push_back(m);
        }
    }

    order_moves(state, captures);

    for (const auto& action : captures) {
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int score;
        if (same) {
            score = quiescence(next, alpha, beta, history, ply + 1, ctx, p);
        } else {
            score = -quiescence(next, -beta, -alpha, history, ply + 1, ctx, p);
        }

        delete next;
        if (ctx.stop) break;

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

/*============================================================
 * AlphaBetaExpert — eval_ctx (with PVS)
 *============================================================*/
int Submission::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const SubmissionParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;

    /* === Lazy move generation === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */
    if (state->game_state == WIN) return P_MAX - ply;
    if (state->game_state == DRAW) return 0;

    int rep_score;
    if(state->check_repetition(history, rep_score)) return rep_score;

    if(depth <= 0){
        if (p.use_quiescence) {
            return quiescence(state, alpha, beta, history, ply, ctx, p);
        }
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    if(state->legal_actions.empty()) {
        return M_MAX + ply;
    }

    /* === PVS (Principal Variation Search) loop === */
    history.push(state->hash());
    int best_score = M_MAX - 1;
    bool first_move = true;

    std::vector<Move> moves = state->legal_actions;
    order_moves(state, moves);

    for(auto& action : moves){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int score;
        if (first_move) {
            // Full window search for the first move
            if (same) {
                score = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
            } else {
                score = -eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
            }
            first_move = false;
        } else {
            // Null window search (Zero Window Search)
            if (same) {
                score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, ply + 1, ctx, p);
            } else {
                score = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p);
            }

            // If null window search fails high, do a full re-search
            if (score > alpha && score < beta) {
                if (same) {
                    score = eval_ctx(next, depth - 1, score, beta, history, ply + 1, ctx, p);
                } else {
                    score = -eval_ctx(next, depth - 1, -beta, -score, history, ply + 1, ctx, p);
                }
            }
        }

        delete next;
        if (ctx.stop) break;

        if (score > best_score) {
            best_score = score;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    history.pop(state->hash());
                    return beta; // Cutoff
                }
            }
        }
    }

    history.pop(state->hash());
    return alpha;
}

SearchResult Submission::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    SubmissionParams p = SubmissionParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()) state->get_legal_actions();
    if(state->legal_actions.empty()) return result;

    int alpha = M_MAX - 10000;
    int beta  = P_MAX + 10000;
    
    std::vector<Move> moves = state->legal_actions;
    order_moves(state, moves);

    int move_index = 0;
    int total_moves = (int)moves.size();
    result.best_move = moves[0];
    result.score = M_MAX - 1;

    bool first_move = true;

    for(auto& action : moves){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int score;
        if (first_move) {
            if (same) {
                score = eval_ctx(next, depth - 1, alpha, beta, history, 1, ctx, p);
            } else {
                score = -eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
            }
            first_move = false;
        } else {
            if (same) {
                score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, 1, ctx, p);
            } else {
                score = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, 1, ctx, p);
            }

            if (score > alpha && score < beta) {
                if (same) {
                    score = eval_ctx(next, depth - 1, score, beta, history, 1, ctx, p);
                } else {
                    score = -eval_ctx(next, depth - 1, -beta, -score, history, 1, ctx, p);
                }
            }
        }

        delete next;
        if(ctx.stop) break;

        if(score > result.score){
            result.score = score;
            result.best_move = action;
            result.pv = {action};

            if(score > alpha){
                alpha = score;
            }

            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, result.score, depth, move_index + 1, total_moves});
            }
        }
        move_index++;
    }

    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    return result;
}

ParamMap Submission::default_params(){
    return {{"UseKPEval", "true"}, {"UseEvalMobility", "true"}, {"ReportPartial", "true"}, {"UseQuiescence", "true"}};
}

std::vector<ParamDef> Submission::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
        {"UseQuiescence", ParamDef::CHECK, "true"}
    };
}
