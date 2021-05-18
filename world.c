#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <string.h>

#include "message.h"
#include "logging.c"

typedef struct {
    char is_alive;
    char ex[20];
    char symbol;
    coordinate pos;
    int fd[2]; // 0 -> world, 1 -> monster
    pid_t pid;
} monster;

typedef struct {
    char executable[20];
    coordinate pos;
    int fd[2]; // 0 -> world, 1 -> player
    pid_t pid;
} hero;

hero player;
monster** monsters;
int room[2]; // x, y
coordinate door;
int num_monsters;
int alive_monsters;
int game_over;
int max_turn;
game_over_status status;
int not_left;


monster** start_the_game(){
    monster** monsters;
    int i;
    char x_door[10], y_door[10];
    char player_1[10], player_2[10], player_3[10];
    char monster_1[10], monster_2[10], monster_3[10], monster_4[10];
    pid_t pid;


    // read room properties
    scanf("%d %d", &room[0], &room[1]);
    scanf("%s %s", x_door, y_door);
    door.x = strtol(x_door, NULL, 10);
    door.y = strtol(y_door, NULL, 10);
    

    // read player properties
    scanf("%d %d %s %s %s %s", &player.pos.x, &player.pos.y, player.executable, player_1, player_2, player_3);
    max_turn = strtol(player_3, NULL, 10);
    socketpair(AF_UNIX, SOCK_STREAM, 0, player.fd);

    // fork player
    pid = fork();
    if (pid < 0){
        printf("player fork() error...\n");
    }
    else if (pid == 0){
        close(player.fd[0]);
        dup2(player.fd[1], STDIN_FILENO);
        dup2(player.fd[1], STDOUT_FILENO);
        execl(player.executable, player.executable, x_door, y_door, player_1, player_2, player_3, (char *) 0);
        printf("player execl() error error...\n"); 
    }
    else {
        close(player.fd[1]);
        player.pid = pid;
    }

    // initialize monsters
    scanf("%d", &num_monsters);

    monsters = malloc(sizeof(monster*) * num_monsters);
    for(i = 0; i < num_monsters; i++){
        monsters[i] = malloc(sizeof(monster) * num_monsters);
    }

    for(i = 0; i < num_monsters; i++){
        scanf("%s %c %d %d %s %s %s %s", 
         monsters[i]->ex,
        &monsters[i]->symbol,
        &monsters[i]->pos.x,
        &monsters[i]->pos.y,
        monster_1,
        monster_2,
        monster_3,
        monster_4);
        socketpair(AF_UNIX, SOCK_STREAM, 0, monsters[i]->fd);
        monsters[i]->is_alive = 1;
        
        pid = fork();

        if (pid < 0){
            printf("monster %d fork() error...\n", i);
        }
        else if (pid == 0){ // Monster
            close(monsters[i]->fd[0]);
            dup2(monsters[i]->fd[1], STDIN_FILENO);
            dup2(monsters[i]->fd[1], STDOUT_FILENO);
            execl(monsters[i]->ex, monsters[i]->ex, monster_1, monster_2, monster_3, monster_4, (char *) 0);
            printf("monster %d execl() error...\n", i);
        }
        else { // World
            close(monsters[i]->fd[1]);
            monsters[i]->pid = pid;
        }

    }
    return monsters;
}

void print_map_world(){
    map_info map;
    int i;
    for(i = 0; i < alive_monsters; i++){
        map.monster_types[i] = monsters[i]->symbol;
        map.monster_coordinates[i] = monsters[i]->pos;
    }
    map.map_width = room[0];
    map.map_height = room[1];
    map.door = door;
    map.player = player.pos;
    map.alive_monster_count = alive_monsters;

    print_map(&map);   
}

void insertion_sort_monsters(){
    int cur, tmp;
    monster* cur_monster;

    for(cur = 1; cur < alive_monsters; cur++){
        tmp = cur;
        cur_monster = monsters[cur];
        while(tmp > 0 && monsters[tmp-1]->pos.y > cur_monster->pos.y){
            monsters[tmp] = monsters[tmp-1];
            tmp--;
        }
        monsters[tmp] = cur_monster;
    }

    for(cur = 1; cur < alive_monsters; cur++){
        tmp = cur;
        cur_monster = monsters[cur];
        while(tmp > 0 && monsters[tmp-1]->pos.x > cur_monster->pos.x){
            monsters[tmp] = monsters[tmp-1];
            tmp--;
        }
        monsters[tmp] = cur_monster;
    }
}

void send_message_to_player(int damage){
    int i;
    player_message message;
    message.new_position = player.pos;
    message.total_damage = damage;
    message.alive_monster_count = alive_monsters;
    for(i = 0; i < alive_monsters; i++){
        message.monster_coordinates[i].x = monsters[i]->pos.x;
        message.monster_coordinates[i].y = monsters[i]->pos.y;
    }
    message.game_over = game_over;

    write(player.fd[0], &message, sizeof(player_message));
}

int receive_message_from_player(player_response* response){
    return read(player.fd[0], response, sizeof(player_response));
}

void send_game_over_message_to_monster(int i){
    monster_message mes;
    mes.game_over = 1;
    write(monsters[i]->fd[0], &mes, sizeof(mes));
}

void send_message_to_monster(int i, int damage){
    monster_message mes;
    mes.new_position = monsters[i]->pos;
    mes.damage = damage;
    mes.player_coordinate = player.pos;
    mes.game_over = game_over;

    write(monsters[i]->fd[0], &mes, sizeof(monster_message));
}

void receive_message_from_monster(monster_response* response, int i){
    read(monsters[i]->fd[0], response, sizeof(monster_response));
}

int is_player_at_the_door(){
    return player.pos.x == door.x && player.pos.y == door.y;
}

int is_on_the_wall(coordinate point){
    return point.x == 0 || point.y == 0 || point.x == room[0] - 1 || point.y == room[1] - 1;
}

int is_anybody_on(coordinate point){
    int is_it = 0;
    int i;
    for(i = 0; i < alive_monsters; i++){
        if (monsters[i]->pos.x == point.x && monsters[i]->pos.y == point.y){
            is_it = 1;
            break;
        }
    }


    return is_it;
}

int is_movable(coordinate point){
    return !is_on_the_wall(point) && !is_anybody_on(point);
}

void destroy_dead_monsters(){
    int i, j;
    for(i = 0, j = 0; i < alive_monsters; i++){
        if(monsters[i]->is_alive){
            monsters[j] = monsters[i];
            j++;
        }
    }
    alive_monsters = j;
    if (!alive_monsters){
        game_over = 1;
        status = go_survived;
    }
}

void end_game(int is_left){

    player_message end_player;
    int i;   
    
    end_player.game_over = 1;

    if(is_left){
        write(player.fd[0], &end_player, sizeof(player_message));
    }
    waitpid(player.pid, NULL, 0);
    
    for(i = 0; i < alive_monsters; i++){
        send_game_over_message_to_monster(i);
        waitpid(monsters[i]->pid, NULL, 0);
    }
    
    print_game_over(status);    
}

int main(int argc, char* argv[]){
    int i;

    player_response player_received;
    monster_response monster_received;

    monsters = start_the_game();
    
    alive_monsters = num_monsters;

    print_map_world();

    // Receive ready messages
    receive_message_from_player(&player_received);
    while(player_received.pr_type != pr_ready){
        printf("Player is not ready, trying again ...\n");
        receive_message_from_player(&player_received);
    }


    for(i = 0; i < alive_monsters; i++){
        receive_message_from_monster(&monster_received, i);
        //printf("Monster %d : %d\n", i, monster_received);
        while(monster_received.mr_type != mr_ready){
            printf("Monster %d is not ready, trying again ...\n", i);
            receive_message_from_monster(&monster_received, i);
        }
    }

    //printf("Player, and Monsters are ready...\n");

    // Game loop
    int received_damage = 0;
    game_over = 0;
    while(!game_over){
        int attack_flag = 0;        
        
        insertion_sort_monsters(monsters, alive_monsters);
        
        send_message_to_player(received_damage);
        received_damage = 0;
        not_left = receive_message_from_player(&player_received);
        if (!not_left){
            game_over = 1;
            status = go_left;
            print_map_world();
            break;
        }
        switch (player_received.pr_type){
            case pr_move:
                player.pos = player_received.pr_content.move_to;
                if (is_player_at_the_door()){
                    game_over = 1;
                    status = go_reached;
                }
                break;
            case pr_attack:
                attack_flag = 1;
                break;
            case pr_dead:
                game_over = 1;
                status = go_died;
                break;    
            default:
                printf("Shouldn't have happened, ending game\n");
                game_over = 1;
                status = go_left;
                break;
        }

        if (!game_over){
            for(i = 0; i < alive_monsters; i++){
                send_message_to_monster(i, attack_flag ? player_received.pr_content.attacked[i] : 0);
                receive_message_from_monster(&monster_received, i);
                
                switch (monster_received.mr_type){
                    case mr_move:
                        if(is_movable(monster_received.mr_content.move_to)){
                            monsters[i]->pos = monster_received.mr_content.move_to;
                        }
                        break;
                    case mr_attack:
                        received_damage += monster_received.mr_content.attack;
                        break;
                    case mr_dead:
                        send_game_over_message_to_monster(i);
                        waitpid(monsters[i]->pid, NULL, 0);
                        monsters[i]->is_alive = 0;
                        break;
                    default:
                        printf("Shouldn't have happened, ending game\n");
                        game_over = 1;
                        status = go_left;
                        break;
                }
            }
        }

        destroy_dead_monsters();

        print_map_world();
    }

    end_game(not_left);


    for(i = 0; i < num_monsters; i++){
        free(monsters[i]);
    }
    free(monsters);


    return 0;
}