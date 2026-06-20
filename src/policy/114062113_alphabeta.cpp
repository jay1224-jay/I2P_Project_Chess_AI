#include <utility>
#include <algorithm>
#include "114062113_state.hpp"
#include "114062113_alphabeta.hpp"

int AlphaBeta::eval_ctx(
    State *state, int depth, int alpha, int beta,
    GameHistory& history, int ply, SearchContext& ctx, const ABParams& p
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

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        
        int score;
        if (same) {
            score = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
        } else {
            score = -eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
        }
        delete next;

        if (score > best_score) {
            best_score = score;
            if (best_score > alpha) alpha = best_score;
            if (alpha >= beta) {
                break; // Alpha-Beta Cutoff
            }
        }
    }

    history.pop(state->hash());
    
    if (best_score < M_MAX) return M_MAX + ply;
    return best_score;
}

SearchResult AlphaBeta::search(
    State *state, int depth, GameHistory& history, SearchContext& ctx
){
    ctx.reset();
    ABParams p = ABParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()) state->get_legal_actions();
    if(state->legal_actions.empty()) return result;

    int alpha = M_MAX - 10000;
    int beta  = P_MAX + 10000;
    int best_score = M_MAX - 1;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();
    
    result.best_move = state->legal_actions[0];

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        
        int score;
        if (same) {
            score = eval_ctx(next, depth - 1, alpha, beta, history, 1, ctx, p);
        } else {
            score = -eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
        }
        delete next;

        if(score > best_score){
            best_score = score;
            result.best_move = action;
            result.score = score;
            result.pv = {action};

            if (best_score > alpha) alpha = best_score;

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

ParamMap AlphaBeta::default_params(){
    return {{"UseKPEval", "true"}, {"UseEvalMobility", "true"}, {"ReportPartial", "true"}};
}

std::vector<ParamDef> AlphaBeta::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"}
    };
}