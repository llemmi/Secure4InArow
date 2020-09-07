//
// Created by Laura Lemmi on 07/09/2020.
//

#ifndef ALL_GAMEBOARD_H
#define ALL_GAMEBOARD_H

#include <cstdint>

class GameBoard {
private:
    int **gameMatrix;

    int insertCoordinateInBoard(uint8_t, uint8_t);
    bool iHaveWon();
    bool iHaveLost();
public:
    GameBoard();
    int insertOpponentMove(uint8_t, u_int8_t);
    int insertMyMove(uint8_t, uint8_t);


};


#endif //ALL_GAMEBOARD_H