#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "message.h"



void send_ready(){
    monster_response ready;
    ready.mr_type = mr_ready;
    ready.mr_content.attack = 0;
    write(STDOUT_FILENO, &ready, sizeof(monster_response));
}

int distance(coordinate c1, coordinate c2){
    return abs(c1.x - c2.x) + abs(c1.y - c2.y);
}



int main(int argc, char* argv[]){
    int health, damage, defence, range;

    monster_message received_mes;
    monster_response sent_mes;

    int xs[8] = {-1, -1, -1, 0, 1, 1, 1, 0};
    int ys[8] = {1, 0, -1, -1, -1, 0, 1, 1};

    if (argc < 5) return -1;

    health = strtol(argv[1], NULL, 10);
    damage = strtol(argv[2], NULL, 10);
    defence = strtol(argv[3], NULL, 10);
    range = strtol(argv[4], NULL, 10);

    send_ready();

    while(1){
        read(STDIN_FILENO, &received_mes, sizeof(monster_message));

        if (received_mes.game_over) break;

        health -= received_mes.damage - defence > 0 ? (received_mes.damage - defence) : 0;
        if (health <= 0){ // Died
            sent_mes.mr_type = mr_dead;
            sent_mes.mr_content.attack = 0;
            write(STDOUT_FILENO, &sent_mes, sizeof(monster_response));
        }
        else {
            int dis = distance(received_mes.player_coordinate, received_mes.new_position);
            
            if(dis <= range){ // can attack
                sent_mes.mr_type = mr_attack;
                sent_mes.mr_content.attack = damage;
                write(STDOUT_FILENO, &sent_mes, sizeof(monster_response));
            }
            else { // move closer
                coordinate tmp = {received_mes.new_position.x + xs[0], received_mes.new_position.y + ys[0]};
                int min_dis = distance(tmp, received_mes.player_coordinate);
                coordinate min_coor = tmp;
                int cur_dis;
                int i;
                
                for(i = 0; i < 8; i++){
                    coordinate tmp = {received_mes.new_position.x + xs[i], received_mes.new_position.y + ys[i]};
                    cur_dis = distance(tmp, received_mes.player_coordinate);
                    if (cur_dis <= min_dis){
                        min_dis = cur_dis;
                        min_coor = tmp;
                    }                    
                }
                sent_mes.mr_type = mr_move;
                sent_mes.mr_content.move_to = min_coor;
                write(STDOUT_FILENO, &sent_mes, sizeof(monster_response));
            }
        }   
              
    }
    return 0;
}