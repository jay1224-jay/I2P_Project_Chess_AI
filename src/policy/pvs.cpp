#include <utility>
#include <algorithm>
#include <vector>
#include "state.hpp"
#include "pvs.hpp"

/* Move Ordering Helper (crucial for PVS efficiency) */
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
            score = 1000 + victim; // Capture heuristic
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

int PVS::eval_ctx(
    State *state, int depth, int alpha, int beta,
    GameHistory& history, int ply, SearchContext& ctx, const PVSParams& p
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

    if(state->legal_actions.empty()) return M_MAX + ply;

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
            if (same) score = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
            else      score = -eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
            first_move = false;
        } else {
            // Null Window Search
            if (same) score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, ply + 1, ctx, p);
            else      score = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p);

            // Re-search if null window fails high
            if (score > alpha && score < beta) {
                if (same) score = eval_ctx(next, depth - 1, score, beta, history, ply + 1, ctx, p);
                else      score = -eval_ctx(next, depth - 1, -beta, -score, history, ply + 1, ctx, p);
            }
        }
        delete next;
        if (ctx.stop) break;

        if (score > best_score) {
            best_score = score;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    break; // Cutoff
                }
            }
        }
    }

    history.pop(state->hash());
    return best_score;
}

SearchResult PVS::search(
    State *state, int depth, GameHistory& history, SearchContext& ctx
){
    ctx.reset();
    PVSParams p = PVSParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(state->legal_actions.empty()) state->get_legal_actions();
    if(state->legal_actions.empty()) return result;

    int alpha = M_MAX - 10000;
    int beta  = P_MAX + 10000;
    
    std::vector<Move> moves = state->legal_actions;
    order_moves(state, moves);

    int best_score = M_MAX - 1;
    int move_index = 0;
    int total_moves = (int)moves.size();
    result.best_move = moves[0];
    bool first_move = true;

    for(auto& action : moves){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        
        int score;
        if (first_move) {
            if (same) score = eval_ctx(next, depth - 1, alpha, beta, history, 1, ctx, p);
            else      score = -eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
            first_move = false;
        } else {
            if (same) score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, 1, ctx, p);
            else      score = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, 1, ctx, p);

            if (score > alpha && score < beta) {
                if (same) score = eval_ctx(next, depth - 1, score, beta, history, 1, ctx, p);
                else      score = -eval_ctx(next, depth - 1, -beta, -score, history, 1, ctx, p);
            }
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

ParamMap PVS::default_params(){
    return {{"UseKPEval", "true"}, {"UseEvalMobility", "true"}, {"ReportPartial", "true"}};
}

std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"}
    };
}