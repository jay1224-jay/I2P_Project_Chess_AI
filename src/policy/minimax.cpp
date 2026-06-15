#include <utility>
#include <algorithm>
#include "state.hpp"
#include "minimax.hpp"

/*============================================================
 * MiniMax — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
int MiniMax::eval_ctx(
    State *state,
    int depth,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */
    if (state->game_state == WIN) {
        return P_MAX - ply;
    }
    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }

    if(depth <= 0){
        return state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    }

    /* === Negamax loop === */
    history.push(state->hash());
    int best_score = M_MAX - 1;
    
    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        
        int score = eval_ctx(next, depth - 1, history, ply + 1, ctx, p);
        
        if (!same) {
            score = -score;
        }
        delete next;

        if (score > best_score) {
            best_score = score;
        }
    }

    history.pop(state->hash());
    
    /* If no moves were made (and not handled by terminal checks), it's a loss */
    if (best_score < M_MAX) {
        return M_MAX + ply;
    }

    return best_score;
}


/*============================================================
 * MiniMax — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult MiniMax::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()){
        state->get_legal_actions();
    }
    if(state->legal_actions.empty()){
        return result;
    }

    int best_score = M_MAX - 1;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    result.best_move = state->legal_actions[0]; // fallback

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        
        int score = eval_ctx(next, depth - 1, history, 1, ctx, p);
        
        if (!same) {
            score = -score;
        }
        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;
            result.score = score;
            result.pv.clear();
            result.pv.push_back(action);

            if(p.report_partial && ctx.on_root_update){
                ctx.on_root_update({result.best_move, result.score, depth, move_index + 1, total_moves});
            }
        }  
        move_index++;
        if(ctx.stop) break;
    }

    result.nodes = ctx.nodes;
    result.seldepth = ctx.seldepth;
    return result;
} 


/*============================================================
 * MiniMax — default_params / param_defs
 *============================================================*/
ParamMap MiniMax::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> MiniMax::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
