#include <utility>
#include <algorithm>
#include "state.hpp"
#include "alphabeta.hpp"

/*============================================================
 * AlphaBeta — eval_ctx
 *
 * Negamax with Alpha-Beta pruning.
 *============================================================*/
int AlphaBeta::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const ABParams& p
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
        // TODO: Implement Quiescence Search here
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    /* === Alpha-Beta Negamax loop === */
    history.push(state->hash());
    int best_score = M_MAX - 1;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        // TODO: Implement the recursive Alpha-Beta call
        // Remember to handle 'same' player turns and flip alpha/beta for negamax
        int score = 0; 

        delete next;
        if(ctx.stop) break;

        // TODO: Update best_score, alpha, and perform pruning (beta cutoff)
    }

    history.pop(state->hash());
    if (best_score < M_MAX) return M_MAX + ply;

    return best_score;
}

SearchResult AlphaBeta::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    ABParams p = ABParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()) state->get_legal_actions();
    if(state->legal_actions.empty()) return result;

    int alpha = M_MAX - 10;
    int beta  = P_MAX + 10;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    result.best_move = state->legal_actions[0];

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        // TODO: Initial root search call
        int score = 0;

        delete next;
        if(ctx.stop) break;

        if(score > alpha){
            alpha = score;
            result.best_move = action;
            result.score = score;
            result.pv = {action}; // Basic PV

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

ParamMap AlphaBeta::default_params(){
    return {{"UseKPEval", "true"}, {"UseEvalMobility", "true"}, {"ReportPartial", "true"}};
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {{"UseKPEval", ParamDef::CHECK, "true"}, {"UseEvalMobility", ParamDef::CHECK, "true"}, {"ReportPartial", ParamDef::CHECK, "true"}};
}
