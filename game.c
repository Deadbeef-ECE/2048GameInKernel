/** @file game.c
 *  @brief A kernel with timer, keyboard, console support
 *
 *  This file contains the kernel's main() function.
 *
 *  It sets up the drivers and starts the game.
 *
 *  @author Yuhang Jiang (yuhangj)
 *  @bug No known bugs.
 */

#include <p1kern.h>

/* libc includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <simics.h>                 /* lprintf() */
#include <malloc.h>

/* multiboot header file */
#include <multiboot.h>              /* boot_info */

/* memory includes. */
#include <lmm.h>                    /* lmm_remove_free() */

/* x86 specific includes */
#include <x86/seg.h>                /* install_user_segs() */
#include <x86/interrupt_defines.h>  /* interrupt_setup() */
#include <x86/asm.h>                /* enable_interrupts() */

#include <string.h>

/* Macros for mode selection */
#define MODE128  'z'
#define MODE256  'x'
#define MODE512  'c'
#define MODE1024 'v'
#define MODE2048 'b'

/* Macros for oprations */
#define UP       'w' 
#define LEFT     'a' 
#define RIGHT    'd'  
#define DOWN     's' 
#define PAUSE    'p'
#define QUIT     'q'
#define RESTART  'r'

/* Game buffer size */
#define SIZE 4
/* Location for printing the number on the real board */
#define X(x)    (x * 11 + 6)
#define Y(y)    (y * 6 + 3)

/* Location for printing the number on the pseudo-board */
#define PSEUDO_X(x)    (x * 11 + 6)
#define PSEUDO_Y(y)    (y * 6 + 1)

/* Location for print current score */
#define SCORE_X 4
#define SCORE_Y 54

/* Location for mode */
#define MODE_X 15
#define MODE_Y 49

/* Location for print best score */
#define BESTSCORE_X 4
#define BESTSCORE_Y 70

/* Location for pause or resume */
#define PAUSE_X 12
#define PAUSE_Y 3

/* Location for timer */
#define TIME_X 23
#define TIME_Y 49


/* Macros for generating random number, rand() from stdlib.h */
#define RANDOM(x)      (rand()%x)
#define RANDONM_NUM(x) ((((rand()%x)/(x-1))+1)*2)

/* Several global variables for the game */
int score = 0;
int best_score = 0;
int target_score;
int seconds = 0;
int pause = 0;
uint16_t psd_board[SIZE][SIZE]={{0}};

void game_init();

/* Game operation functions */
void add_random(uint16_t board[SIZE][SIZE]);
int move_up(uint16_t board[SIZE][SIZE]);
int move_down(uint16_t board[SIZE][SIZE]);
int move_left(uint16_t board[SIZE][SIZE]);
int move_right(uint16_t board[SIZE][SIZE]);
void rotate(uint16_t board[SIZE][SIZE]);

/* Game over/win helper functions */
int find_same(uint16_t board[SIZE][SIZE]);
int is_over(uint16_t board[SIZE][SIZE]);
int is_win(uint16_t board[SIZE][SIZE]);
int game_win();
int game_over();

/* Other help functions */
void set_target_score();
int set_color(int num);
void draw_num(uint16_t board[SIZE][SIZE]);
void clear_num(uint16_t board[SIZE][SIZE]);
void draw_psd_num(uint16_t psd_board[SIZE][SIZE]);
void hide_psd_num(uint16_t psd_board[SIZE][SIZE]);
void print_score();
void print_mode();
void print_bestscore();
void animation(uint16_t board[SIZE][SIZE], uint16_t psd_board[SIZE][SIZE]);
void copy_borad(uint16_t board[SIZE][SIZE], uint16_t psd_board[SIZE][SIZE]);

void debug_print(uint16_t board[SIZE][SIZE]);

void tick(unsigned int numTicks);

/** @brief Kernel entrypoint.
 *  
 *  This is the entrypoint for the kernel.  It simply sets up the
 *  drivers and passes control off to game_run().
 *
 *  @return Does not return
 */
int kernel_main(mbinfo_t *mbinfo, int argc, char **argv, char **envp)
{
    /*
     * Initialize device-driver library.
     */
    

    /*
     * When kernel_main() begins, interrupts are DISABLED.
     * You should delete this comment, and enable them --
     * when you are ready.
     */

    lprintf( "Hello from a brand new kernel!" );
    game_init();

    while(1)
        continue;
}

/** @brief Tick function, to be called by the timer interrupt handler
 * 
 *  In a real game, this function would perform processing which
 *  should be invoked by timer interrupts.
 *  Lock the number when pause has been triggered
 *
 *  @return void
 */
void tick(unsigned int numTicks)
{  
    if(numTicks %100 == 0){
        if(pause == 0){
            seconds ++;
        }
            set_cursor(TIME_X, TIME_Y);
            set_term_color(FGND_BCYAN);
            printf("TIME: %d", seconds);
    }

}

/** @brief Welcome page
 *
 *  Inclueds instructions and provides five mode for the game.
 */
static const char *welcome = 
"                                                                                "
"                                                                                "
"   Welcome to my 2048 game!                                                     "
"                                                                                "
"   Instructions:                                                                "
"   Use 'w'(up) 'a'(left) 's'(down) 'd'(right) to move the numbers. When two     "
"   numbers with same value touch, they merge into one!                          "
"   When you are playing the game, you can use 'p' to pause,'q' to quit, and     "
"   'r' to restart the game.                                                     "
"   When you win or lose the game, just follow the instruction to restart or     "
"   quit the game.                                                               "
"                                                                                "
"   First, please select your target score:                                      "
"                                                                                "
"   'z': 128 mode    'x': 256 mode                                               "
"   'c': 512 mode    'v': 1024 mode                                              "
"   'b': 2048 mode                                                               "
"                                                      @Author: Yuhang Jiang     "
"                                                        @Andrew ID: yuhangj     "
"                                                                                "
"                                              ";

/** @brief UI for game board
 *
 *  Inclueds simple instructions and shows your currest&best score.
 */
static const char *UI = 
" +----------+----------+----------+----------+  +--------------+---------------+"
" |          |          |          |          |  |    SCORE     |      BEST     |"
" |          |          |          |          |  +--------------+---------------+"
" |          |          |          |          |  |              |               |"
" |          |          |          |          |  |              |               |"
" |          |          |          |          |  |              |               |"
" +----------+----------+----------+----------+  +--------------+---------------+"
" |          |          |          |          |                                  "
" |          |          |          |          |   'w a s d' to move              "
" |          |          |          |          |   'p' to pause                   "
" |          |          |          |          |   'q' to quit                    "
" |          |          |          |          |   'r' to restart                 "
" +----------+----------+----------+----------+                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" +----------+----------+----------+----------+                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" |          |          |          |          |                                  "
" +----------+----------+----------+----------+   ";

/* @brief UI for goodby page */
static const char *BYE = 
"                                                                                "
"                                                                                "
"                                                                                "
"                                                                                "
"                                                                                "
"                    =_==-==.     ||      ||     .=-=_==_                        "
"                    ||      ||     ||    ||     ||                              "
"                   ||      ||      ||  ||      ||                               "
"                    ||      ||       |..|       ||                              "
"                  ||:=_ ==-:         ||        ||:=-_=_-=                       "
"                     ||      ||        ||        ||                             "
"                    ||      ||        ||        ||                              "
"                   ||      ||        ||        ||                               "
"                      ==__==-'         ''         `=_=--_                       "
"                                                                                "
"                   Have a great DAY!!!!                                         "
"                                                                                "
"                                                                                "
"                                                    @Author: Yuhang Jiang       "
"                                                    @Andrew ID: yuhangj         ";

void game_init(){
    uint16_t board[SIZE][SIZE]={{0}};

    char ch;
    int result;
    int gameover, goodbye;
    int win;

    handler_install(tick);
    enable_interrupts();
restartgame:
    /* Clear thr console and (re)set the target score */
    clear_console();
    set_target_score();
    /* Clear the number in the board */
    clear_num(board);
    /* Print the UI for game */
    set_term_color(FGND_BCYAN);
    printf("%s", UI);
    /* Print the selected mode and the best score if not 0 */
    if(best_score!=0)
        print_bestscore();
    print_mode();

    set_cursor(0, 0);
    set_term_color(FGND_WHITE);
    /* Hide the ugly curosr */
    hide_cursor();
    /* Add two random number in the beginning of the game */
    add_random(board);
    add_random(board);
    draw_num(board);
    /* (re)set the time before entering in to the game */
    seconds = 0;
    while(1){
        result = 0;
        goodbye = 0;
        win = 0;
        //int sleep = seconds;
        // while((seconds-sleep)==25){
        //     break;
        // }
        // hide_psd_num(psd_board);
        /* Copy the board into pseudo-board before moving the blocks */
        copy_borad(board, psd_board);
        ch = readchar(); 
        /* Wait for player's instructions, if 'pause' triggered, lock
         * every possible 4 move actions */   
        switch(ch){
            case UP:
                if(pause == 0){
                    result = move_up(board);
                }
                break;
            case DOWN:
                if(pause == 0){
                    result = move_down(board);
                }
                break;
            case LEFT:
                if(pause == 0){
                    result = move_left(board);
                }
                break;
            case RIGHT:
                if(pause == 0){
                    result = move_right(board);
                }
                break;
            case PAUSE:
                /* Trigger 'pause' status and print info on the game UI */
                if(pause == 0){
                    pause = 1;
                    set_cursor(PAUSE_X, PAUSE_Y);
                    set_term_color(FGND_BCYAN);
                    printf("PAUSE! Press 'p' again to resume the game!");
                }else{
                /* Dismiss 'pause' status and print info to recovery the 
                 * part of game UI destroyed by pause info */
                    pause = 0;
                    set_cursor(PAUSE_X, PAUSE_Y);
                    set_term_color(FGND_BCYAN);
                    printf("---------+----------+----------+----------");
                }
                break;
            case QUIT:
                /* Reset the pause flag before showing the 'bye' page */
                if(pause == 1)
                    pause = 0;
                goodbye = 1;
                break;
            case RESTART:
                /* Reset the pause flag before restarting the game */
                if(pause == 1)
                    pause = 0;
                score = 0;
                clear_num(board);
                goto restartgame;
        }
        /* Move actions succeed and draw relavent info on the game UI */
        if(result == 1){
            hide_psd_num(psd_board);
            add_random(board);
            draw_num(board);
            draw_psd_num(psd_board);
            print_score();
            print_bestscore();
        }
        /* Judge win or not */
        if(is_win(board)){
            if(game_win()){
                gameover = 0;
                goodbye = 1;
                break;
            }else{
                score = 0;
                set_cursor(0,0);
                goto restartgame;
            }
        }
        if(goodbye == 1){
            gameover = 0;
            break;
        }
        /* If move actions failed, judge game is over or not */
        if(result == 0 ){
            if(is_over(board)){
                gameover = 1;
                break;
            }
        }
    }
    /* Out of while loop now */
    /* If game is over, then run instructions according 
     * to player's decisions */
    if(gameover == 1){
        goodbye = game_over();
        if(!goodbye){
            score = 0;
            set_cursor(0, 0);
            goto restartgame;
        }
    }
    /* If player want to exit the game, show the 'goodbye' page */
    if(goodbye == 1){
        clear_console();
        set_term_color(FGND_BCYAN);
        printf("%s", BYE);
        return;
    }
}

/** @brief restored the merged value
 *  
 *  Since the slide animation is not obvious, I print the added number
 *  on the top of the orignal ones to show what happens after sliding.
 *
 *  @return void
 */
void animation(uint16_t board[SIZE][SIZE], uint16_t psd_board[SIZE][SIZE]){
    uint16_t back[SIZE][SIZE];
    int n, m;
    copy_borad(psd_board, back);
    int i, j;
    for(i = 0; i < SIZE; i++){
        for(j = 0; j < SIZE; j++){
            if(board[i][j] != 2 * psd_board[i][j])
                psd_board[i][j] = 0;
        }
    }
    for(n = 0; n < SIZE; n++){
        for(m = 0; m < 2; m++){
            if(m == 1){
                if((back[n][2]!=0) && (back[n][3]!=0) &&
                    (board[n][1] == (back[n][2]+back[n][3])))
                    psd_board[n][1] = board[n][1]/2;
            }
            if(m == 0){
                if((back[n][1]!=0) && (back[n][3]!=0) &&
                    (board[n][0] == (back[n][1]+back[n][3])))
                    psd_board[n][0] = board[n][0]/2;

                if((back[n][2]!=0) && (back[n][3]!=0) &&
                    (board[n][0] == (back[n][2]+back[n][3])))
                    psd_board[n][0] = board[n][0]/2;

                if((back[n][2]!=0) && (back[n][1]!=0) &&
                    (board[n][0] == (back[n][2]+back[n][1])))
                    psd_board[n][0] = board[n][0]/2;
            }
        }
    }
    return;
}

/** @brief Copy the contents of the real board into the pseudo-board.
 *  
 *  Called before moving and used to compare the number to record changes.
 *  @param board[SIZE][SIZE]: real board
 *         psd_board[SIZE][SIZE]: pseudo-board
 *  @return void
 */
void copy_borad(uint16_t board[SIZE][SIZE], uint16_t psd_board[SIZE][SIZE]){
    int i, j;
    for(i = 0; i < SIZE; i++){
        for(j = 0; j < SIZE; j++){
            psd_board[i][j] = board[i][j];
        }
    }
    return;
}

/** @brief Set the target socre
 *  
 *  Instuctions for selecting and setting the target score 
 *
 *  @return void
 */
void set_target_score(){
    char select;
    char c;
    set_term_color(FGND_BCYAN);
    printf("%s", welcome);
    hide_cursor();
    while(1){
        target_score = 0;
        select = readchar();
        /* Set the target score */
        switch(select){
            case MODE128:
                target_score = 128;
                break;
            case MODE256:
                target_score = 256;
                break;
            case MODE512:
                target_score = 512;
                break;
            case MODE1024:
                target_score = 1024;
                break;
            case MODE2048:
                target_score = 2048;
                break;
            default:
                continue;
        }
        if(target_score!= 0)
            break;
    }
    /* Wait for selection confirmation and continue the game */
    set_cursor(22, 22);
    printf("You selected '%d' mode! Please type 'y' to continue.\n",
        target_score);
    while(1){
        c = readchar();
        switch(c){
            case 'y':
                clear_console();
                return;
        }
    }
}

/** @brief Print the number of the real board
 *  
 *  Using the location macros to print the contents with the right
 *  place on game board. Print space for 0s and real number for 
 *  non-0s. Need to consider the condition that the shorter number
 *  may cover longer one such as 2 covers 1024, so print more spaces.
 *
 *  @return void
 */
void draw_num(uint16_t board[SIZE][SIZE]){
    int i, j;
    int x = 0;
    int y = 0;
    int color;
    int val = 0;
    for( i = 0; i < SIZE; i++){
        for( j = 0; j < SIZE; j++){
            val = board[i][j];
            x = X(i);
            y = Y(j);
            /* For 0s, print '    ' to cover old contents */
            if(val == 0){
                set_cursor(y, x);
                printf("    ");
            }
            /* Print out real number to cover old contents */
            else{ 
                set_cursor(y, x);
                color = set_color(val);
                set_term_color(color);
                printf("%d   ", val);
            }
        }
    }
    return;
}

/** @brief Print the pseudo-number of the pseudo-board
 *  
 *  Using the location macros to print the added value with the offset
 *  place on game board. Help to show the animation. 
 *
 *  @return void
 */
void draw_psd_num(uint16_t psd_board[SIZE][SIZE]){
    int i, j;
    int x = 0;
    int y = 0;
    int val = 0;
    int color;
    for( i = 0; i < SIZE; i++){
        for( j = 0; j < SIZE; j++){
            val = psd_board[i][j];
            x = PSEUDO_X(i);
            y = PSEUDO_Y(j);
            /* For 0s, print '    ' to cover old contents */
            if(val == 0){
                set_cursor(y, x);
                printf("    ");
            }
            /* Print out added value on the top of new merged number */
            else{
                set_cursor(y, x);
                color = set_color(2*val);
                set_term_color(color);
                printf("+%d ", val);
            }
        }
    }
    return;
}

/** @brief Hide the pseudo-number of the pseudo-board
 *  
 *  Using the location macros to print spaces to cover the added value 
 *  with the offset place on game board. Help to show the animation. 
 *
 *  @return void
 */
void hide_psd_num(uint16_t psd_board[SIZE][SIZE]){
    int i, j;
    int x = 0;
    int y = 0;
    for( i = 0; i < SIZE; i++){
        for( j = 0; j < SIZE; j++){
            x = PSEUDO_X(i);
            y = PSEUDO_Y(j);
            set_cursor(y, x);
            printf("    ");
        }
    }
    return;
}

/** @brief Hide the pseudo-number of the pseudo-board
 *  
 *  Clear the numbers in the board.
 *
 *  @return void
 */
void clear_num(uint16_t board[SIZE][SIZE]){
    int i, j;
    for( i = 0; i < SIZE; i++){
        for( j = 0; j < SIZE; j++){
            board[i][j] = 0;
        }
    }
    return;
}

/** @brief Functions for judge if the game is over or not
 *  
 *  If we cannot find a pair of numbers which can merge into a new one,
 *  then game is over.
 *
 *  @return 0: game is not over
 *          1: game is over
 */
int is_over(uint16_t board[SIZE][SIZE]){
    int over = 1;
    int x, y;
    /* Check if there is empty block */
    for(x = 0; x < SIZE; x++){
        for (y = 0; y < SIZE; y++){
            if(board[x][y] == 0)
                return 0;
        }
    }
    if(find_same(board))
        return 0;
    rotate(board);
    if(find_same(board))
        over = 0;
    rotate(board);
    rotate(board);
    rotate(board);
    return over;
}

/** @brief Functions for finding a pair of number with same value
 *  
 *  If we cannot find a pair of numbers which can merge into a new one,
 *  then game is over.
 *
 *  @return 0: cannot find the pair
 *          1: find the pair
 */
int find_same(uint16_t board[SIZE][SIZE]){
    int x, y;
    for(x = 0; x < SIZE; x++){
        for (y = 0; y < SIZE - 1; y++){
            if(board[x][y] == board[x][y+1])
                return 1;
        }
    }
    return 0;
}


/*  @brief Functions for printing instructions after game over
 *  
 *  Show the game over info, and instruction guide for restarting
 *  the game or quiting the game.
 *
 *  @return 0: player want to restart the game 
 *          1: player want to quit the game
 */
int game_over(){
    int goodbye;
    int restart;
    char ch;
    set_term_color(FGND_BCYAN);
    set_cursor(10, 12);
    printf("BAD LUCK!! GAME IS OVER!");
    set_cursor(14, 20);
    printf("TO RESTART GAME: press 'r'");
    set_cursor(16, 20);
    printf("TO QUIT GAME: press 'q'");
    /* Wait for player to make a decision */
    while(1){
        goodbye = 0;
        restart = 1;
        ch = readchar();
        switch(ch){
            case RESTART:
                restart = 0;
                break;
            case QUIT:
                goodbye = 1;
                break;
            default:
                break;
        }
        if(restart == 0)
            break;
        if(goodbye == 1)
            break;
    }
    if(restart == 0)
        return restart;
    if(goodbye)
        return goodbye;
    return 0;
}

/** @brief Functions for judge if the the play wins the game or not
 *
 *  If there is a number is equal to the target the score, the player
 *  wins the game.
 *
 *  @return 1: win the game
 *          0: not yet
 */
int is_win(uint16_t board[SIZE][SIZE]){
    int win = 0;
    int x, y;
    /* Check if there is an number equals to target */
    for(x = 0; x < SIZE; x++){
        for (y = 0; y < SIZE; y++){
            if(board[x][y] == target_score)
                win = 1;
        }
    }
    return win;
}

/* @brief Functions for printing instructions after win.
 *
 * Show the 'player win' info, and instruction guide for restarting
 * the game or quiting the game 
 *
 * @return 0: player want to restart the game 
 *         1: player want to quit the game
 */
int game_win(){
    int goodbye;
    int restart;
    char ch;
    set_term_color(FGND_BCYAN);
    set_cursor(10, 10);
    printf("GOT %d! GOOD LUCK! YOU WIN!", target_score);
    set_cursor(14, 20);
    printf("TO RESTART GAME: press 'r'");
    set_cursor(16, 20);
    printf("TO QUIT GAME: press 'q'");
    /* Wait for player to make a decision */
    while(1){
        goodbye = 0;
        restart = 1;
        ch = readchar();
        switch(ch){
            case RESTART:
                restart = 0;
                break;
            case QUIT:
                goodbye = 1;
                break;
            default:
                break;
        }
        if(restart == 0)
            break;
        if(goodbye)
            break;
    }
    if(restart == 0)
        return restart;
    if(goodbye)
        return goodbye;
    return 0;
}

/* @brief Functions for adding random number 2 or 4 on the board
 *
 * First, use a 2-D array to record the location of empty numbers
 * on the number board.
 * Then, generate a radom number to select one element in the array.
 * Finally, generate a random number 2 or 4 and store in the board.
 *
 * @return void
 */
void add_random(uint16_t board[SIZE][SIZE]){
    int8_t x, y;
    int16_t random_len, len = 0;
    uint16_t n, list[SIZE * SIZE][2];
    for (x = 0; x < SIZE; x++) {
        for (y = 0; y < SIZE; y++) {
            /* Find and record the empty block(0) */
            if (board[x][y] == 0) {
                /* Store the x, y location of such block */
                list[len][0]=x;
                list[len][1]=y;
                /* Record the number of such empty blocks */
                len++;
            }
        }
    }
    /* If the board is not full */
    if (len > 0) {
        /* Generate random number to select one empty block
         * in the array */
        random_len = RANDOM(len);
        /* Generate the random 2 or 4 */
        n = RANDONM_NUM(3);
        /* Read the location of the selected block and store
         * new random number, then the new number will printed 
         * out by the function draw_num */
        x = list[random_len][0];
        y = list[random_len][1];
        board[x][y]=n;
    }
    return;
}

/* @brief Find the same number before the index x in the array
 *
 * For a single array node in the array, array[x], search through the 
 * array[x-1]->array[0]. 
 * If there is a node whose value is equal to the 0: 
 * Need not merge if t = stop because the array[t] is just the one need
 * to stay.
 * If there is a node whose value is not equal to the 0: 
 * if array[t] is not equal to array[x], return t+1, means cannot merge
 * if array[t] is equal to array[x], which means they can merge, return 
 * the index t.  
 *
 * @return 0: action failed
 *         1: action succeed
 */
int8_t find(uint16_t array[SIZE], int8_t stop, int8_t x){
    int8_t t;
    if(!x){
        return x;
    }
    for(t = x-1; t >= 0; t--){
        if(array[t] == 0){
             if(t == stop)
                return t;
        }else{
           if(array[t] != array[x]){
                return t+1;
            }
            return t;
        }
    }
    return x;
}

/* @brief Functions for move up
 *
 * Move each single array of the 2-D array one by one. And compute the 
 * difference bewteen the new board with the old one. 
 *
 * @return 0: action failed
 *         1: action succeed
 */
int move_array(uint16_t array[SIZE]){
    int i = 0;
    uint8_t y, t, stop = 0;
    for(y = 0; y < SIZE; y++){
        if(array[y] != 0){
            t = find(array, stop, y);
            /* If we find a node is not in the same place, we
             * can move or merge */
            if(t != y){
                if(array[t]!=0){
                    score += array[t] + array[y];
                    /* Update the best score */
                    if(best_score <= score)
                        best_score = score;
                    /* If fine one which is not 0, set stop index in order
                     * to prevent invalid merge */
                    stop = t + 1;
                }
                array[t]+= array[y];
                array[y] = 0;
                i = 1;
            }
        }
    }
    return i;
}

/* @brief Functions for move up
 *
 * Move each single array of the 2-D array one by one. And compute the 
 * difference bewteen the new board with the old one. 
 *
 * @return 0: action failed
 *         1: action succeed
 */
int move_up(uint16_t board[SIZE][SIZE]){
    int i = 0;
    int x;
    for(x = 0; x < SIZE; x++){
        i |= move_array(board[x]);
    }
    animation(board,psd_board);
    return i;
}

/* @brief Functions for move down
 *
 * All these three move actions takes similar theory. Rotate the board 
 * into move up direction. After than, rotate back.
 *
 * @return 0: action failed
 *         1: action succeed
 */
int move_down(uint16_t board[SIZE][SIZE]){
    int i = 0;
    rotate(board);
    rotate(psd_board);
    rotate(board);
    rotate(psd_board);
    i = move_up(board);
    rotate(board);
    rotate(psd_board);
    rotate(board);
    rotate(psd_board);
    return i;
}

/* @brief Functions for move left
 *
 * All these three move actions takes similar theory. Rotate the board 
 * into move up direction. After than, rotate back.
 *
 * @return 0: action failed
 *         1: action succeed
 */
int move_left(uint16_t board[SIZE][SIZE]){
    int i = 0;
    rotate(board);
    rotate(psd_board);
    i = move_up(board);
    rotate(board);
    rotate(psd_board);
    rotate(board);
    rotate(psd_board);
    rotate(board);
    rotate(psd_board);
    return i;
}

/* @brief Functions for move right
 *
 * All these three move actions takes similar theory. Rotate the board 
 * into move up direction. After than, rotate back.
 *
 * @return 0: action failed
 *         1: action succeed
 */
int move_right(uint16_t board[SIZE][SIZE]){
    int i = 0;
    rotate(board);
    rotate(psd_board);
    rotate(board);
    rotate(psd_board);
    rotate(board);
    rotate(psd_board);
    i = move_up(board);
    rotate(board);
    rotate(psd_board);
    return i;
}

/* @brief Functions for rotate the board
 *
 * We can just simply to rotate the board into the direction operated 
 * by move up action, and keep rotating the board to the orignal 
 * direction
 *
 * @return void
 */
void rotate(uint16_t board[SIZE][SIZE]){
    int i, j;
    int n = SIZE;
    int m = SIZE - 1;
    uint16_t temp;
    for(i = 0; i < n/2; i ++){
        for(j = i; j < m - i; j ++){
            temp = board[i][j];
            board[i][j] = board[j][m-i];
            board[j][m-i] = board[m-i][m-j];
            board[m-i][m-j] = board[m-j][i];
            board[m-j][i] = temp;
        }
    }
    return;
}

/* @brief Functions for debugging the board number
 *
 * Print out the content of the number in the terminal for debugging.
 *
 * @return void
 */
void debug_print(uint16_t board[SIZE][SIZE]){
    int x = 0;
    lprintf("DEBUG BOARD");
    for(; x < SIZE; x++){
        lprintf("[%d][%d][%d][%d]", board[x][0],
            board[x][1],board[x][2],board[x][3]);
    }
    return;
}

/* @brief Functions for set the color of the number
 *
 * Set the color according to the value of a number.
 *
 * @param  num: the value needed to add color
 * @return color: the value of the color
 */
int set_color(int num){
    int color = 0;
    switch(num){
        case 2:
            color = FGND_WHITE;
            break;
        case 4:
            color = FGND_YLLW;
            break;
        case 8:
            color = FGND_BMAG;
            break;
        case 16:
            color = FGND_BGRN;
            break;
        case 32:
            color = FGND_BBLUE;
            break;
        case 64:
            color = FGND_BRWN;
            break;
        case 128:
            color = FGND_MAG;
            break;
        case 256:
            color = FGND_RED;
            break;
        case 512:
            color = FGND_CYAN;
            break;
        case 1024:
            color = FGND_BLUE;
            break;
        case 2048:
            color = FGND_BCYAN;
            break;
    }
    return color;
}

/* @brief Functions for printing the current score 
 *
 * Print the palyer's current score on the UI according the 
 * location macros.
 *
 * @return void
 */
void print_score(){
    set_cursor(SCORE_X, SCORE_Y);
    set_term_color(FGND_BMAG);
    printf("%d    ",  score);
    return;
}

/* @brief Functions for printing the best score 
 *
 * Print the palyer's best score on the UI according the 
 * location macros.
 *
 * @return void
 */
void print_bestscore(){
    set_cursor(BESTSCORE_X, BESTSCORE_Y);
    set_term_color(FGND_BCYAN);
    printf("%d    ",  best_score);
    return;
}

/* @brief Functions for printing the target score 
 *
 * Print the selected target score on the UI according the 
 * location macros.
 *
 * @return void
 */
void print_mode(){
    set_cursor(MODE_X, MODE_Y);
    set_term_color(FGND_BCYAN);
    printf("In '%d' Mode ", target_score);
    return;
}













