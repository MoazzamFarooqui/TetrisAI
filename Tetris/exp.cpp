// tetris_raylib.cpp
// Single-file Tetris-like game using raylib (C++).
// Features: Main menu, 10x20 board, 7-bag randomizer, rotation, collision, line clear, next-piece preview, scoring, level speed.
// Controls:
//  - Left / Right arrows: move piece
//  - Down arrow: soft drop
//  - Up arrow or X: rotate clockwise
//  - Z: rotate counter-clockwise
//  - Space: hard drop
//  - P: pause
// Build (Linux):
//   g++ tetris_raylib.cpp -o tetris -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
// Build (Windows - MSYS2):
//   g++ tetris_raylib.cpp -o tetris.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include <raylib.h>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <chrono>
#include <string>

using namespace std;

// Board dimensions
const int BOARD_W = 10;
const int BOARD_H = 20;
const int CELL = 24;
const int WINDOW_W = 640;
const int WINDOW_H = 720;

// Game states
enum GameState {
    MAIN_MENU,
    PLAYING,
    GAME_OVER
};

// Tetromino definitions (4x4 matrices)
static const array<array<int,16>,7> TETROMINO = {
    // I
    array<int,16>{0,0,0,0,
                  1,1,1,1,
                  0,0,0,0,
                  0,0,0,0},
    // O
    array<int,16>{0,1,1,0,
                  0,1,1,0,
                  0,0,0,0,
                  0,0,0,0},
    // T
    array<int,16>{0,1,0,0,
                  1,1,1,0,
                  0,0,0,0,
                  0,0,0,0},
    // J
    array<int,16>{1,0,0,0,
                  1,1,1,0,
                  0,0,0,0,
                  0,0,0,0},
    // L
    array<int,16>{0,0,1,0,
                  1,1,1,0,
                  0,0,0,0,
                  0,0,0,0},
    // S
    array<int,16>{0,1,1,0,
                  1,1,0,0,
                  0,0,0,0,
                  0,0,0,0},
    // Z
    array<int,16>{1,1,0,0,
                  0,1,1,0,
                  0,0,0,0,
                  0,0,0,0}
};

static const array<Color,8> PALETTE = {
    BLACK, SKYBLUE, YELLOW, MAGENTA, BLUE, ORANGE, GREEN, RED
};

struct Piece {
    int type; // 0..6
    int x, y; // position refers to top-left of 4x4 area relative to board
    int rotation; // 0..3
};

// Board: 0 empty, 1..7 filled with tetromino id +1
struct Game {
    array<array<int, BOARD_W>, BOARD_H> board{};
    Piece cur;
    vector<int> bag; // upcoming queue
    int score = 0;
    int lines = 0;
    int level = 1;
    bool gameOver = false;
    bool paused = false;
    std::mt19937 rng;
    
    Game(){
        rng.seed((unsigned)chrono::high_resolution_clock::now().time_since_epoch().count());
        refillBag();
        spawnPiece();
    }

    void refillBag(){
        bag.clear();
        for(int i=0;i<7;i++) bag.push_back(i);
        shuffle(bag.begin(), bag.end(), rng);
    }

    int nextFromBag(){
        if(bag.empty()) refillBag();
        int v = bag.back(); bag.pop_back();
        return v;
    }

    void spawnPiece(){
        cur.type = nextFromBag();
        cur.rotation = 0;
        cur.x = (BOARD_W/2) - 2;
        cur.y = 0;
        if(collides(cur.x, cur.y, cur.type, cur.rotation)){
            gameOver = true;
        }
    }

    int pieceCell(int type, int rotation, int i, int j) const {
        int idx = 0;
        switch(rotation % 4){
            case 0: idx = i*4 + j; break;
            case 1: idx = (3 - j)*4 + i; break;
            case 2: idx = (3 - i)*4 + (3 - j); break;
            case 3: idx = j*4 + (3 - i); break;
        }
        return TETROMINO[type][idx];
    }

    bool collides(int px, int py, int type, int rotation) const {
        for(int i=0;i<4;i++){
            for(int j=0;j<4;j++){
                if(pieceCell(type, rotation, i, j)){
                    int bx = px + j;
                    int by = py + i;
                    if(bx < 0 || bx >= BOARD_W || by < 0 || by >= BOARD_H) return true;
                    if(board[by][bx] != 0) return true;
                }
            }
        }
        return false;
    }

    void lockPiece(){
        for(int i=0;i<4;i++){
            for(int j=0;j<4;j++){
                if(pieceCell(cur.type, cur.rotation, i, j)){
                    int bx = cur.x + j;
                    int by = cur.y + i;
                    if(by >= 0 && by < BOARD_H && bx >=0 && bx < BOARD_W)
                        board[by][bx] = cur.type + 1;
                }
            }
        }
        clearLines();
        spawnPiece();
    }

    void clearLines(){
        int cleared = 0;
        for(int r = BOARD_H - 1; r >= 0; r--){
            bool full = true;
            for(int c=0;c<BOARD_W;c++) if(board[r][c] == 0){ full = false; break; }
            if(full){
                cleared++;
                for(int rr = r; rr > 0; rr--){
                    board[rr] = board[rr-1];
                }
                board[0].fill(0);
                r++;
            }
        }
        if(cleared > 0){
            lines += cleared;
            static const int pointsPer[5] = {0,40,100,300,1200};
            score += pointsPer[cleared] * level;
            level = 1 + lines / 10;
        }
    }

    void hardDrop(){
        while(!collides(cur.x, cur.y+1, cur.type, cur.rotation)) cur.y++;
        lockPiece();
    }
};

struct MenuItem {
    string text;
    Rectangle bounds;
    Color color;
};

void DrawMainMenu(int& selectedOption, float animTime) {
    ClearBackground(Color{20, 20, 40, 255});
    
    // Title with animation
    const char* title = "TETRIS";
    int titleSize = 80;
    int titleWidth = MeasureText(title, titleSize);
    int titleY = 120 + (int)(sin(animTime * 2) * 5);
    
    // Shadow
    DrawText(title, WINDOW_W/2 - titleWidth/2 + 4, titleY + 4, titleSize, Fade(BLACK, 0.5f));
    // Main title with gradient effect (simulated with multiple draws)
    DrawText(title, WINDOW_W/2 - titleWidth/2, titleY, titleSize, SKYBLUE);
    
    // Falling block animation in background
    for(int i = 0; i < 7; i++) {
        int xPos = 50 + i * 85;
        int yPos = (int)(100 + fmod(animTime * 80 + i * 60, WINDOW_H + 100)) % (WINDOW_H + 100) - 50;
        DrawRectangle(xPos, yPos, 30, 30, Fade(PALETTE[i+1], 0.3f));
    }
    
    // Menu options
    vector<string> options = {"Start Game", "Instructions", "Quit"};
    int startY = 320;
    int spacing = 80;
    
    for(size_t i = 0; i < options.size(); i++) {
        int optionY = startY + i * spacing;
        int textWidth = MeasureText(options[i].c_str(), 30);
        int textX = WINDOW_W/2 - textWidth/2;
        
        bool isSelected = (selectedOption == (int)i);
        Color textColor = isSelected ? YELLOW : WHITE;
        
        // Selected highlight
        if(isSelected) {
            DrawRectangle(textX - 20, optionY - 10, textWidth + 40, 50, Fade(SKYBLUE, 0.3f));
            DrawText(">", textX - 50, optionY, 30, YELLOW);
        }
        
        DrawText(options[i].c_str(), textX, optionY, 30, textColor);
    }
    
    // Footer
    DrawText("Use UP/DOWN arrows and ENTER to select", WINDOW_W/2 - MeasureText("Use UP/DOWN arrows and ENTER to select", 16)/2, 
             WINDOW_H - 80, 16, LIGHTGRAY);
}

void DrawInstructions(bool& showInstructions) {
    ClearBackground(Color{20, 20, 40, 255});
    
    DrawText("INSTRUCTIONS", WINDOW_W/2 - MeasureText("INSTRUCTIONS", 40)/2, 60, 40, SKYBLUE);
    
    int startY = 140;
    int lineHeight = 35;
    
    vector<pair<string, string>> instructions = {
        {"LEFT/RIGHT", "Move piece"},
        {"DOWN", "Soft drop"},
        {"UP or X", "Rotate clockwise"},
        {"Z", "Rotate counter-clockwise"},
        {"SPACE", "Hard drop"},
        {"P", "Pause game"},
        {"ENTER/R", "Restart (Game Over)"}
    };
    
    for(size_t i = 0; i < instructions.size(); i++) {
        int y = startY + i * lineHeight;
        DrawText(instructions[i].first.c_str(), 120, y, 20, YELLOW);
        DrawText("-", 280, y, 20, WHITE);
        DrawText(instructions[i].second.c_str(), 310, y, 20, WHITE);
    }
    
    // Game info
    DrawText("OBJECTIVE:", 120, startY + 280, 24, SKYBLUE);
    DrawText("Clear lines by filling rows completely.", 120, startY + 315, 18, LIGHTGRAY);
    DrawText("Game speeds up every 10 lines.", 120, startY + 345, 18, LIGHTGRAY);
    DrawText("Don't let blocks reach the top!", 120, startY + 375, 18, LIGHTGRAY);
    
    // Back instruction
    DrawText("Press ENTER to return to menu", WINDOW_W/2 - MeasureText("Press ENTER to return to menu", 18)/2, 
             WINDOW_H - 60, 18, YELLOW);
}

int main(){
    InitWindow(WINDOW_W, WINDOW_H, "Tetris");
    SetTargetFPS(60);

    GameState gameState = MAIN_MENU;
    int selectedMenuOption = 0;
    bool showInstructions = false;
    float animTime = 0.0f;

    Game game;
    float gravityTimer = 0.0f;
    float gravityDelay = 0.8f;
    float inputDelay = 0.08f;
    float inputTimer = 0.0f;
    bool leftHeld = false, rightHeld = false, downHeld = false;

    while(!WindowShouldClose()){
        animTime += GetFrameTime();
        
        // Main Menu State
        if(gameState == MAIN_MENU) {
            if(!showInstructions) {
                if(IsKeyPressed(KEY_UP)) {
                    selectedMenuOption = (selectedMenuOption - 1 + 3) % 3;
                }
                if(IsKeyPressed(KEY_DOWN)) {
                    selectedMenuOption = (selectedMenuOption + 1) % 3;
                }
                if(IsKeyPressed(KEY_ENTER)) {
                    if(selectedMenuOption == 0) {
                        // Start Game
                        game = Game();
                        gameState = PLAYING;
                        gravityTimer = 0;
                        inputTimer = 0;
                    } else if(selectedMenuOption == 1) {
                        // Instructions
                        showInstructions = true;
                    } else if(selectedMenuOption == 2) {
                        // Quit
                        break;
                    }
                }
            } else {
                // In instructions
                if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
                    showInstructions = false;
                }
            }
            
            BeginDrawing();
            if(showInstructions) {
                DrawInstructions(showInstructions);
            } else {
                DrawMainMenu(selectedMenuOption, animTime);
            }
            EndDrawing();
            continue;
        }

        // Playing State
        if(IsKeyPressed(KEY_P)) game.paused = !game.paused;
        
        if(game.gameOver){
            if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R)){
                game = Game();
                gravityTimer = 0; 
                inputTimer = 0;
            }
            if(IsKeyPressed(KEY_ESCAPE)) {
                gameState = MAIN_MENU;
                selectedMenuOption = 0;
            }
        }

        if(!game.gameOver && !game.paused){
            float dt = GetFrameTime();
            gravityDelay = std::max(0.05f, 0.8f - (game.level-1)*0.05f);
            gravityTimer += dt;
            inputTimer += dt;

            if(IsKeyDown(KEY_LEFT)){
                if(!leftHeld || inputTimer >= inputDelay){
                    if(!game.collides(game.cur.x - 1, game.cur.y, game.cur.type, game.cur.rotation)){
                        game.cur.x -= 1;
                    }
                    leftHeld = true; inputTimer = 0;
                }
            } else leftHeld = false;

            if(IsKeyDown(KEY_RIGHT)){
                if(!rightHeld || inputTimer >= inputDelay){
                    if(!game.collides(game.cur.x + 1, game.cur.y, game.cur.type, game.cur.rotation)){
                        game.cur.x += 1;
                    }
                    rightHeld = true; inputTimer = 0;
                }
            } else rightHeld = false;

            if(IsKeyDown(KEY_DOWN)){
                if(!downHeld || inputTimer >= inputDelay){
                    if(!game.collides(game.cur.x, game.cur.y+1, game.cur.type, game.cur.rotation)){
                        game.cur.y += 1;
                    }
                    downHeld = true; inputTimer = 0;
                }
            } else downHeld = false;

            if(IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)){
                int newRot = (game.cur.rotation + 1) % 4;
                if(!game.collides(game.cur.x, game.cur.y, game.cur.type, newRot)){
                    game.cur.rotation = newRot;
                } else {
                    if(!game.collides(game.cur.x-1, game.cur.y, game.cur.type, newRot)) game.cur.x-- , game.cur.rotation = newRot;
                    else if(!game.collides(game.cur.x+1, game.cur.y, game.cur.type, newRot)) game.cur.x++ , game.cur.rotation = newRot;
                }
            }
            if(IsKeyPressed(KEY_Z)){
                int newRot = (game.cur.rotation + 3) % 4;
                if(!game.collides(game.cur.x, game.cur.y, game.cur.type, newRot)){
                    game.cur.rotation = newRot;
                } else {
                    if(!game.collides(game.cur.x-1, game.cur.y, game.cur.type, newRot)) game.cur.x-- , game.cur.rotation = newRot;
                    else if(!game.collides(game.cur.x+1, game.cur.y, game.cur.type, newRot)) game.cur.x++ , game.cur.rotation = newRot;
                }
            }

            if(IsKeyPressed(KEY_SPACE)){
                game.hardDrop();
                gravityTimer = 0;
            }

            if(gravityTimer >= gravityDelay){
                gravityTimer = 0;
                if(!game.collides(game.cur.x, game.cur.y+1, game.cur.type, game.cur.rotation)){
                    game.cur.y += 1;
                } else {
                    game.lockPiece();
                }
            }
        }

        // Draw Game
        BeginDrawing();
        ClearBackground(BLACK);

        int boardX = 20, boardY = 20;
        DrawRectangle(boardX-4, boardY-4, BOARD_W*CELL+8, BOARD_H*CELL+8, DARKGRAY);
        DrawRectangle(boardX, boardY, BOARD_W*CELL, BOARD_H*CELL, LIGHTGRAY);

        for(int r=0;r<BOARD_H;r++){
            for(int c=0;c<BOARD_W;c++){
                int v = game.board[r][c];
                if(v){
                    DrawRectangle(boardX + c*CELL, boardY + r*CELL, CELL-2, CELL-2, PALETTE[v]);
                }
            }
        }

        if(!game.gameOver){
            for(int i=0;i<4;i++){
                for(int j=0;j<4;j++){
                    if(game.pieceCell(game.cur.type, game.cur.rotation, i, j)){
                        int bx = game.cur.x + j;
                        int by = game.cur.y + i;
                        if(by >= 0){
                            DrawRectangle(boardX + bx*CELL, boardY + by*CELL, CELL-2, CELL-2, PALETTE[game.cur.type+1]);
                        }
                    }
                }
            }
        }

        for(int i=0;i<=BOARD_W;i++) DrawLine(boardX + i*CELL, boardY, boardX + i*CELL, boardY + BOARD_H*CELL, Fade(BLACK,0.12f));
        for(int i=0;i<=BOARD_H;i++) DrawLine(boardX, boardY + i*CELL, boardX + BOARD_W*CELL, boardY + i*CELL, Fade(BLACK,0.12f));

        int sidebarX = boardX + BOARD_W*CELL + 20;
        int sidebarY = boardY;
        DrawText(TextFormat("Score: %d", game.score), sidebarX, sidebarY, 20, BLACK);
        DrawText(TextFormat("Lines: %d", game.lines), sidebarX, sidebarY + 28, 18, BLACK);
        DrawText(TextFormat("Level: %d", game.level), sidebarX, sidebarY + 52, 18, BLACK);

        DrawText("Next:", sidebarX, sidebarY + 90, 18, BLACK);
        int nx = sidebarX; int ny = sidebarY + 120;
        
        vector<int> preview;
        auto tmpBag = game.bag;
        std::mt19937 tmpRng = game.rng;
        while(preview.size() < 5){
            if(tmpBag.empty()){
                array<int,7> seeds = {0,1,2,3,4,5,6};
                std::shuffle(seeds.begin(), seeds.end(), tmpRng);
                for(int k=0;k<7;k++) tmpBag.push_back(seeds[k]);
            }
            preview.push_back(tmpBag.back()); tmpBag.pop_back();
        }
        for(size_t pi=0; pi<preview.size(); pi++){
            int t = preview[pi];
            for(int i=0;i<4;i++){
                for(int j=0;j<4;j++){
                    if(game.pieceCell(t,0,i,j)){
                        DrawRectangle(nx + j*12 + 40, ny + i*12 + pi*60, 10, 10, PALETTE[t+1]);
                    }
                }
            }
        }

        DrawText("Arrows: Move/Drop", sidebarX, sidebarY + 420, 12, WHITE);
        DrawText("Up/X: Rotate  Z: CCW", sidebarX, sidebarY + 440, 12, WHITE);
        DrawText("Space: Hard Drop", sidebarX, sidebarY + 460, 12, WHITE);
        DrawText("P: Pause  ESC: Menu", sidebarX, sidebarY + 480, 12, WHITE);

        if(game.paused){
            DrawRectangle(0, WINDOW_H/2 - 40, WINDOW_W, 80, Fade(BLACK, 0.5f));
            DrawText("Paused", WINDOW_W/2 - MeasureText("Paused", 40)/2, WINDOW_H/2 - 20, 40, WHITE);
        }

        if(game.gameOver){
            DrawRectangle(0, WINDOW_H/2 - 60, WINDOW_W, 120, Fade(BLACK, 0.6f));
            DrawText("Game Over", WINDOW_W/2 - MeasureText("Game Over", 40)/2, WINDOW_H/2 - 36, 40, RED);
            DrawText(TextFormat("Score: %d  Lines: %d", game.score, game.lines), WINDOW_W/2 - MeasureText("Score", 20)/2, WINDOW_H/2 + 6, 20, WHITE);
            DrawText("Enter/R: Restart  ESC: Menu", WINDOW_W/2 - MeasureText("Enter/R: Restart  ESC: Menu", 18)/2, WINDOW_H/2 + 36, 18, LIGHTGRAY);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}