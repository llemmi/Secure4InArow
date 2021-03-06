//
// Created by Laura Lemmi on 07/09/2020.
//

#include "GameBoard.h"


GameBoard::GameBoard(const char* me,const char* other) {

    for(int i = 0; i < ROWSNUMBER; i++) {
        for(int j = 0; j < COLUMNSNUMBER; j++)
            gameMatrix[i][j] = -1;
    }
    this->myUserName.append(me);
    this->opponentUserName.append(other);

}

GameBoard::~GameBoard() {

    for(int i = 0; i < ROWSNUMBER; i++) {
        for(int j = 0; j < COLUMNSNUMBER; j++)
            gameMatrix[i][j] = -1;
    }
}


int GameBoard::insertCoordinateInBoard(uint8_t column, int value) {

    if(column < 1 || column > 7) {
        std::cout << "Error! Coordinate not valid" << std::endl;
        return -1;
    }
    column--;


    //obtain first free cell in column
    int row = this->getFirstFreeCellInColumn(column);

    if(row < 0) {
        std::cout<<"Error! That colum is full!"<<std::endl;
        return -2;
    }

    if(gameMatrix[row][column] != -1){
        std::cout<<"Error! That position was not empty!"<<std::endl;
        return -1;
    }

    gameMatrix[row][column] = value;
    return 0;

}

int GameBoard::insertMyMove(uint8_t y) {

    int ret = insertCoordinateInBoard(y, 0);
    if(ret ==-2) return ret;
    if(ret == -1) {
        std::cout<<"Some error occured"<<std::endl;
        return ret;
    }
    //check if I won
    if(gameFinished(0)) {
        return 1;
    }
    //check if I lost
    if(gameFinished(1)) {
        return 2;
    }
    return 0;
}

int GameBoard::insertOpponentMove( uint8_t y) {
    int ret = insertCoordinateInBoard(y, 1);

    if(ret == -1) {
        std::cout<<"Some error occured"<<std::endl;
        return ret;
    }

    //check if I won
    if(gameFinished(0)) {
        return 1;
    }
    //check if I lost
    if(gameFinished(1)) {
        return 2;
    }
    return 0;
}

bool GameBoard::gameFinished(int valueToCheck) {

    for(int i = 0; i < ROWSNUMBER; i++){    //rows
        for(int j = 0; j < COLUMNSNUMBER; j++){     //colums
                //check horizontal
            if (j + 3 < COLUMNSNUMBER) {
                if (this->gameMatrix[i][j] == valueToCheck &&
                    this->gameMatrix[i][j + 1] == valueToCheck &&
                    this->gameMatrix[i][j + 2] == valueToCheck &&
                    this->gameMatrix[i][j + 3] == valueToCheck) {
                    return true;
                }
            }

            // check vertical
            if (i + 3 < ROWSNUMBER) {
                if (this->gameMatrix[i][j] == valueToCheck &&
                    this->gameMatrix[i + 1][j] == valueToCheck &&
                    this->gameMatrix[i + 2][j] == valueToCheck &&
                    this->gameMatrix[i + 3][j] == valueToCheck) {
                    return true;
                }
            }

            //check diagonal from top left corner
            if (i + 3 < ROWSNUMBER && j + 3 < COLUMNSNUMBER) {
                if (this->gameMatrix[i][j] == valueToCheck &&
                    this->gameMatrix[i + 1][j + 1] == valueToCheck &&
                    this->gameMatrix[i + 2][j + 2] == valueToCheck &&
                    this->gameMatrix[i + 3][j + 3] == valueToCheck) {
                    return true;
                }
            }

            //check diagonal from top right corner
            if (i + 3 < ROWSNUMBER && j - 3 >= 0) {
                if (this->gameMatrix[i][j] == valueToCheck &&
                    this->gameMatrix[i + 1][j - 1] == valueToCheck &&
                    this->gameMatrix[i + 2][j - 2] == valueToCheck &&
                    this->gameMatrix[i + 3][j - 3] == valueToCheck) {
                    return true;
                }
            }
        }
    }
    return false;
}



std::ostream &operator<<(std::ostream &out,const GameBoard& g) {

    string rowColors("echo \"||");

    unsigned int index = 0;
    unsigned int nextPos = g.CELLWIDTH-1;
    unsigned int totalNumberOfequals = (g.CELLWIDTH +2)* g.GAMEBOARDCOLUMNS +2;


    string header( "echo You:\e[42m   \e[0m  " +  g.opponentUserName + ":\e[41m   \e[0m ");
    system(header.c_str());

    for(int j = 0;j < totalNumberOfequals;j++){
        if(j == nextPos){
            out<<index+1;
            index++;
            nextPos += 2*(g.CELLWIDTH-1) -1;
        }
        else{
            out<<" ";
        }

    }
    out<<endl;

    for(int i =0;i< (g.CELLWIDTH +2)* g.GAMEBOARDCOLUMNS +2;i++){
        out<<"=";
    }
    out<<endl;

    for (int i = 0; i < g.GAMEBOARDROWS; ++i)
    {
        for (int j = 0; j < g.GAMEBOARDCOLUMNS; ++j){
            switch(g.gameMatrix[i][j]){
                case -1:rowColors.append(g.BLACKPIXEL);break;
                case 0 :rowColors.append(g.GREENPIXEL);break;
                case 1 :rowColors.append(g.REDPIXEL);break;
            }
        }

        rowColors.append("\"");
        system(rowColors.c_str());

        string temp(rowColors);
        temp.replace(temp.end()-1,temp.end(),std::to_string(COLUMNSNUMBER-(i+1)));
        temp.append("\"");

        system(temp.c_str());
        system(rowColors.c_str());

        rowColors.clear();
        temp.clear();
        rowColors.append("echo \"||");

        for(int k =0;k< (g.CELLWIDTH +2)* g.GAMEBOARDCOLUMNS +2;k++){
            out<<"=";
        }
        out<<endl;

    }
    return out;
}

int GameBoard::getFirstFreeCellInColumn(uint8_t column) {
    for(int row = 5; row >= 0; row--){
        if(this->gameMatrix[row][column] == -1)
            return row;
    }
    return -1;
}



