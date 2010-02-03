/*
 *	search2.c
 *	A collection of related search functions for the
 *	parallex chess program.
*/

#include "common.h"

/* This function can be seen as the public interface to the search method.
 * This is called in the main function, and like its name suggests, it tries
 * to pick the best move. It calls the search function.
 */

int mdepth; //Max depth
extern int iProc;
extern int nProc;

int pickbestmove(board_t board, int side)
{
	int i;
    if (!endgame)
    {
	int piececount = 0;
	for (i = 0; i < 64; i++)
		if (board.piece[i] != EMPTY)
			piececount++;
	if (piececount < PC_ENDGAME)
        {
		endgame = 1;
		printf("\n\nEnd Game has been reached.\n");
        }
    }

	memset(pv, 0, sizeof(pv));

	for (i = 0; i < depth[side]; i++)
	{
		mdepth = depth[side];
		int ab[2];
		if (iProc == 0) search_pv(&ab[0], &ab[1], alpha[side], beta[side], i, board, side);
		MPI_Bcast( ab, 2, MPI_INT, 0, MPI_COMM_WORLD);
		search(ab[0], ab[1], i, board, side);
	}


    if (pv[depth[side]].u == -20000)   //No moves, game is over
	    return -1;

    return pv[depth[side]].u;
}

int search_pv(int *a, int *b, int alpha, int beta, int depth, board_t board, int side)
{
	int lastmove, i, move_score;
	board_t newmove;
	
	if (!depth)
		return eval(board, side);
	
	movestack legal_moves[MAX_MOVES];	//data to hold all of the pseudo-legal moves for this board
	
	/*******************************MOVE GENERATION*******************************/
	lastmove = genmoves(board, legal_moves, side);	//generate and store all of the pseudo-legal moves
	
	sort_pv(legal_moves, depth, lastmove);
	
	for (i = 0; i < lastmove; i++) {
        sort(legal_moves, lastmove);
		if(!makeourmove(board, legal_moves[i].m.b, &newmove, side))	//Make the move, store it into newmove. Test for legality
			continue;	//If this move isn't legal, move onto the next one
		break;
	}
	
	move_score = -search_pv(a,b,-beta, -alpha, depth - 1, newmove, side ^ 1);	//Search again with this move to see opponent's responses
		
		
	if (move_score >= beta)
		return beta;
		
	if (move_score > alpha)
	{
		pv[depth] = legal_moves[i].m;
		alpha = move_score;
	}
	*a = alpha;
	*b = beta;
	
	return alpha;
}


int search(int alpha, int beta, int depth, board_t board, int side)
{
	int lastmove, i, move_score;
	board_t newmove;

        if (!depth)
            return eval(board, side);

	movestack legal_moves[MAX_MOVES];	//data to hold all of the pseudo-legal moves for this board

	/*******************************MOVE GENERATION*******************************/
	lastmove = genmoves(board, legal_moves, side);	//generate and store all of the pseudo-legal moves

	sort_pv(legal_moves, depth, lastmove);

	int startIndex = iProc * lastmove / nProc;
	int stopIndex = (iProc+1) * lastmove / nProc;
	if (iProc == nProc - 1) stopIndex = lastmove;
	
	for (i = startIndex; i < stopIndex; i++) {
        sort(legal_moves, lastmove);
		if(!makeourmove(board, legal_moves[i].m.b, &newmove, side))	//Make the move, store it into newmove. Test for legality
			continue;	//If this move isn't legal, move onto the next one

		move_score = -search(-beta, -alpha, depth - 1, newmove, side ^ 1);	//Search again with this move to see opponent's responses
		
		if (depth == mdepth) MPI_Allreduce(&alpha, &alpha, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

		if (move_score >= beta)
                    return beta;

		if (move_score > alpha)
		{
			pv[depth] = legal_moves[i].m;
			alpha = move_score;
		}
	} //end for()
	
	

	return alpha;
}

/* sort() searches the current ply's move list
 to the end to find the move with the highest score. Then it
 swaps that move and the 'from' move so the move with the
 highest score gets searched next, and hopefully produces
 a cutoff. */

void sort(movestack *moves, int last_move)
{
	int i;
	int bs;  /* best score */
	int bi;  /* best i */
	movestack g;

	bs = -1;
	bi = 0;
	for (i = 0; i < last_move; ++i)
		if (moves[i].score > bs) {
			bs = moves[i].score;
			bi = i;
		}
	g = moves[0];
	moves[0] = moves[bi];
	moves[bi] = g;

}

void sort_pv(movestack *moves, int depth, int lastmove)
{
	int i;

	for (i = 0; i < lastmove; ++i)
		if (moves[i].m.u == pv[depth].u)
			moves[i].score += 10000000;
}