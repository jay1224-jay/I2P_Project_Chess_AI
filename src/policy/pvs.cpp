#include <utility>
#include <algorithm>
#include "state.hpp"
#include "pvs.hpp"

/*============================================================
 * PVS — eval_ctx
 *
 * Principal Variation Search (PVS).
 *============================================================*/
int PVS::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const PVSParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth) ctx.seldepth = ply;
    if(ctx.stop) return 0;

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if (state->game_state == WIN) return P_MAX - ply;
    if (state->game_state == DRAW) return 0;

    int rep_score;
    if(state->check_repetition(history, rep_score)) return rep_score;

    if(depth <= 0){
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    history.push(state->hash());
    int best_score = M_MAX - 1;
    bool first_child = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();

        int score;
        if (first_child) {
            // TODO: Full window search for the first move (PV)
            score = 0;
            first_child = false;
        } else {
            // TODO: Null window search for remaining moves (ZW)
            // If ZW fails, perform a full re-search
            score = 0;
        }

        delete next;
        if(ctx.stop) break;

        // TODO: Update alpha and prune
    }

    history.pop(state->hash());
    if (best_score < M_MAX) return M_MAX + ply;
    return best_score;
}

SearchResult PVS::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    PVSParams p = PVSParams::from_map(ctx.params);
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
        
        // TODO: Root PVS logic
        int score = 0;

        delete next;
        if(ctx.stop) break;

        if(score > alpha){
            alpha = score;
            result.best_move = action;
            result.score = score;
            result.pv = {action};

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

ParamMap PVS::default_params(){
    return {{"UseKPEval", "true"}, {"UseEvalMobility", "true"}, {"ReportPartial", "true"}};
}

std::vector<ParamDef> PVS::param_defs(){
    return {{"UseKPEval", ParamDef::CHECK, "true"}, {"UseEvalMobility", ParamDef::CHECK, "true"}, {"ReportPartial", ParamDef::CHECK, "true"}};
}
