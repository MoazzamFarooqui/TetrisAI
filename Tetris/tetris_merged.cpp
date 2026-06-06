#include <raylib.h>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <chrono>
#include <string>
#include <limits>
#include <iostream>
#include <climits>

using namespace std;

// ==================== CONSTANTS ====================
const int BOARD_W = 10;
const int BOARD_H = 20;
const int CELL = 24;
const int WINDOW_W = 640;
const int WINDOW_H = 720;

// AI Heuristic weights
const double W_LINES = 0.760666;
const double W_HOLE = -0.35663;
const double W_HEIGHT = -0.510066;
const double W_BUMPY = -0.184483;

// ==================== ENUMS ====================
enum GameState {
    MAIN_MENU,
    MODE_SELECT,
    PLAYING,
    GAME_OVER
};

enum GameMode {
    MANUAL,
    AI
};

// ==================== TETROMINO DEFINITIONS ====================
using Matrix4 = std::array<std::array<int,4>,4>;

static const array<array<int,16>,7> TETROMINO_CLASSIC = {
    // I
    array<int,16>{0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0},
    // O
    array<int,16>{0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0},
    // T
    array<int,16>{0,1,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
    // J
    array<int,16>{1,0,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
    // L
    array<int,16>{0,0,1,0, 1,1,1,0, 0,0,0,0, 0,0,0,0},
    // S
    array<int,16>{0,1,1,0, 1,1,0,0, 0,0,0,0, 0,0,0,0},
    // Z
    array<int,16>{1,1,0,0, 0,1,1,0, 0,0,0,0, 0,0,0,0}
};

static const array<Color,8> PALETTE = {
    BLACK, SKYBLUE, YELLOW, MAGENTA, BLUE, ORANGE, GREEN, RED
};

struct Tetromino {
    vector<Matrix4> states;
    int colorId;
};

// ==================== UTILITY FUNCTIONS ====================
Matrix4 rotate90(const Matrix4 &m) {
    Matrix4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[i][j] = m[3 - j][i];
    return r;
}

vector<Tetromino> makeTetrominoes() {
    vector<Tetromino> T(7);
    
    // I
    Matrix4 I0 = {{
        {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}
    }};
    T[0].states = {I0, rotate90(I0)};
    T[0].colorId = 1;

    // J
    Matrix4 J0 = {{
        {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}
    }};
    for (int i=0;i<4;i++) {
        if (i==0) T[1].states.push_back(J0);
        else T[1].states.push_back(rotate90(T[1].states.back()));
    }
    T[1].colorId = 4;

    // L
    Matrix4 L0 = {{
        {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}
    }};
    for (int i=0;i<4;i++) {
        if (i==0) T[2].states.push_back(L0);
        else T[2].states.push_back(rotate90(T[2].states.back()));
    }
    T[2].colorId = 5;

    // O
    Matrix4 O0 = {{
        {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}
    }};
    T[3].states = {O0};
    T[3].colorId = 2;

    // S
    Matrix4 S0 = {{
        {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}
    }};
    T[4].states = {S0, rotate90(S0)};
    T[4].colorId = 6;

    // T
    Matrix4 T0m = {{
        {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}
    }};
    for (int i=0;i<4;i++) {
        if (i==0) T[5].states.push_back(T0m);
        else T[5].states.push_back(rotate90(T[5].states.back()));
    }
    T[5].colorId = 3;

    // Z
    Matrix4 Z0 = {{
        {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}
    }};
    T[6].states = {Z0, rotate90(Z0)};
    T[6].colorId = 7;

    return T;
}

// ==================== PIECE STRUCTURE ====================
struct Piece {
    int type;
    int x, y;
    int rotation;
};

// ==================== BOARD CLASS ====================
class Board {
public:
    array<array<int, BOARD_W>, BOARD_H> cells{};

    Board() {
        for (auto &row : cells) row.fill(0);
    }

    bool inBounds(int r, int c) const {
        return r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W;
    }

    bool collides(const Matrix4 &shape, int topR, int leftC) const {
        for (int i=0;i<4;i++) {
            for (int j=0;j<4;j++) {
                if (!shape[i][j]) continue;
                int r = topR + i;
                int c = leftC + j;
                if (c < 0 || c >= BOARD_W || r >= BOARD_H) return true;
                if (r >= 0 && cells[r][c] != 0) return true;
            }
        }
        return false;
    }

    void placePiece(const Matrix4 &shape, int topR, int leftC, int colorId) {
        for (int i=0;i<4;i++) {
            for (int j=0;j<4;j++) {
                if (shape[i][j]) {
                    int r = topR + i;
                    int c = leftC + j;
                    if (r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W)
                        cells[r][c] = colorId;
                }
            }
        }
    }

    int clearLines() {
        int cleared = 0;
        for (int r = BOARD_H - 1; r >= 0; --r) {
            bool full = true;
            for (int c = 0; c < BOARD_W; ++c)
                if (cells[r][c] == 0) { full = false; break; }
            if (full) {
                cleared++;
                for (int rr = r; rr > 0; --rr)
                    cells[rr] = cells[rr-1];
                cells[0].fill(0);
                r++;
            }
        }
        return cleared;
    }

    int dropPosition(const Matrix4 &shape, int leftC) const {
        int topR = -4;
        while (!collides(shape, topR + 1, leftC)) topR++;
        if (collides(shape, topR, leftC)) return INT_MIN;
        return topR;
    }

    int columnHeight(int c) const {
        for (int r = 0; r < BOARD_H; ++r)
            if (cells[r][c] != 0) return BOARD_H - r;
        return 0;
    }

    int aggregateHeight() const {
        int s = 0;
        for (int c=0;c<BOARD_W;c++) s += columnHeight(c);
        return s;
    }

    int bumpiness() const {
        int b = 0;
        for (int c=0;c<BOARD_W-1;c++)
            b += abs(columnHeight(c) - columnHeight(c+1));
        return b;
    }

    int holes() const {
        int h = 0;
        for (int c=0;c<BOARD_W;c++) {
            bool blockFound = false;
            for (int r=0;r<BOARD_H;r++) {
                if (cells[r][c] != 0) blockFound = true;
                else if (blockFound) h++;
            }
        }
        return h;
    }
};

// ==================== AI DECISION ====================
struct MoveDecision {
    int rotationIndex;
    int leftC;
    double score;
    int lines;
};

MoveDecision findBestMove(const Board &board, const vector<Tetromino> &tetrominoes, int pieceType) {
    const Tetromino &piece = tetrominoes[pieceType];
    MoveDecision best{0,0,-1e9,0};
    
    for (int rIdx = 0; rIdx < (int)piece.states.size(); ++rIdx) {
        const Matrix4 &shape = piece.states[rIdx];
        for (int left = -4; left <= BOARD_W; ++left) {
            int top = board.dropPosition(shape, left);
            if (top == INT_MIN) continue;
            
            Board b2 = board;
            b2.placePiece(shape, top, left, piece.colorId);
            int lines = b2.clearLines();

            double score = 0.0;
            score += W_LINES * lines;
            score += W_HOLE * b2.holes();
            score += W_HEIGHT * b2.aggregateHeight();
            score += W_BUMPY * b2.bumpiness();

            if (score > best.score) {
                best = {rIdx, left, score, lines};
            }
        }
    }
    return best;
}

// ==================== BAG RANDOMIZER ====================
struct Bag {
    vector<int> bag;
    mt19937 rng;
    
    Bag() {
        rng.seed(chrono::high_resolution_clock::now().time_since_epoch().count());
        refill();
    }
    
    void refill() {
        bag.clear();
        for (int i=0;i<7;i++) bag.push_back(i);
        shuffle(bag.begin(), bag.end(), rng);
    }
    
    int next() {
        if (bag.empty()) refill();
        int t = bag.back();
        bag.pop_back();
        return t;
    }
};

// ==================== GAME CLASS ====================
class Game {
public:
    Board board;
    Piece cur;
    Bag bag;
    int score = 0;
    int lines = 0;
    int level = 1;
    bool gameOver = false;
    bool paused = false;
    vector<Tetromino> tetrominoes;
    GameMode mode;
    
    // For manual mode
    float gravityTimer = 0.0f;
    float gravityDelay = 0.8f;
    float inputDelay = 0.08f;
    float inputTimer = 0.0f;
    bool leftHeld = false, rightHeld = false, downHeld = false;
    
    // For AI mode
    float aiTimer = 0.0f;
    float aiCooldown = 1.08f;
    
    Game(GameMode gm) : mode(gm) {
        tetrominoes = makeTetrominoes();
        spawnPiece();
    }
    
    void spawnPiece() {
        cur.type = bag.next();
        cur.rotation = 0;
        cur.x = (BOARD_W/2) - 2;
        cur.y = 0;
        if (collidesPiece(cur.x, cur.y, cur.type, cur.rotation)) {
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
        return TETROMINO_CLASSIC[type][idx];
    }
    
    bool collidesPiece(int px, int py, int type, int rotation) const {
        for(int i=0;i<4;i++){
            for(int j=0;j<4;j++){
                if(pieceCell(type, rotation, i, j)){
                    int bx = px + j;
                    int by = py + i;
                    // Out-of-horizontal-bounds or below board is a collision
                    if(bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
                    // If the cell is within visible board area, check occupancy.
                    if(by >= 0 && board.cells[by][bx] != 0) return true;
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
                        board.cells[by][bx] = cur.type + 1;
                }
            }
        }
        clearLines();
        spawnPiece();
    }
    
    void clearLines(){
        int cleared = board.clearLines();
        if(cleared > 0){
            lines += cleared;
            static const int pointsPer[5] = {0,100,100,300,1200};
            score += pointsPer[cleared] * level;
            level = 1 + lines / 10;
        }
    }
    
    void hardDrop(){
        while(!collidesPiece(cur.x, cur.y+1, cur.type, cur.rotation)) cur.y++;
        lockPiece();
    }
    
    void updateAI(float dt) {
        if(gameOver) return;
        
        aiTimer += dt;
        if(aiTimer >= aiCooldown) {
            aiTimer = 0.0f;
            MoveDecision move = findBestMove(board, tetrominoes, cur.type);
            
            if(move.score < -1e8) {
                gameOver = true;
            } else {
                const Matrix4 &shape = tetrominoes[cur.type].states[move.rotationIndex];
                int top = board.dropPosition(shape, move.leftC);
                if (top == INT_MIN) {
                    gameOver = true;
                } else {
                    board.placePiece(shape, top, move.leftC, tetrominoes[cur.type].colorId);
                    int cleared = board.clearLines();
                    if (cleared > 0) {
                        lines += cleared;
                        score += 100 * (1 << (cleared - 1));
                        level = 1 + lines / 10;
                    }
                    spawnPiece();
                }
            }
        }
    }
    
    void updateManual(float dt) {
        if(gameOver || paused) return;
        
        gravityDelay = max(0.05f, 0.8f - (level-1)*0.05f);
        gravityTimer += dt;
        inputTimer += dt;

        if(IsKeyDown(KEY_LEFT)){
            if(!leftHeld || inputTimer >= inputDelay){
                if(!collidesPiece(cur.x - 1, cur.y, cur.type, cur.rotation)){
                    cur.x -= 1;
                }
                leftHeld = true; inputTimer = 0;
            }
        } else leftHeld = false;

        if(IsKeyDown(KEY_RIGHT)){
            if(!rightHeld || inputTimer >= inputDelay){
                if(!collidesPiece(cur.x + 1, cur.y, cur.type, cur.rotation)){
                    cur.x += 1;
                }
                rightHeld = true; inputTimer = 0;
            }
        } else rightHeld = false;

        if(IsKeyDown(KEY_DOWN)){
            if(!downHeld || inputTimer >= inputDelay){
                if(!collidesPiece(cur.x, cur.y+1, cur.type, cur.rotation)){
                    cur.y += 1;
                }
                downHeld = true; inputTimer = 0;
            }
        } else downHeld = false;

        if(IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_X)){
            int newRot = (cur.rotation + 1) % 4;
            if(!collidesPiece(cur.x, cur.y, cur.type, newRot)){
                cur.rotation = newRot;
            } else {
                if(!collidesPiece(cur.x-1, cur.y, cur.type, newRot)) cur.x-- , cur.rotation = newRot;
                else if(!collidesPiece(cur.x+1, cur.y, cur.type, newRot)) cur.x++ , cur.rotation = newRot;
            }
        }
        
        if(IsKeyPressed(KEY_Z)){
            int newRot = (cur.rotation + 3) % 4;
            if(!collidesPiece(cur.x, cur.y, cur.type, newRot)){
                cur.rotation = newRot;
            } else {
                if(!collidesPiece(cur.x-1, cur.y, cur.type, newRot)) cur.x-- , cur.rotation = newRot;
                else if(!collidesPiece(cur.x+1, cur.y, cur.type, newRot)) cur.x++ , cur.rotation = newRot;
            }
        }

        if(IsKeyPressed(KEY_SPACE)){
            hardDrop();
            gravityTimer = 0;
        }

        if(gravityTimer >= gravityDelay){
            gravityTimer = 0;
            if(!collidesPiece(cur.x, cur.y+1, cur.type, cur.rotation)){
                cur.y += 1;
            } else {
                lockPiece();
            }
        }
    }
};

// ==================== RENDERING ====================
void DrawMainMenu(int& selectedOption, float animTime) {
    ClearBackground(Color{20, 20, 40, 255});
    
    const char* title = "TETRIS";
    int titleSize = 80;
    int titleWidth = MeasureText(title, titleSize);
    int titleY = 120 + (int)(sin(animTime * 2) * 5);
    
    DrawText(title, WINDOW_W/2 - titleWidth/2 + 4, titleY + 4, titleSize, Fade(BLACK, 0.5f));
    DrawText(title, WINDOW_W/2 - titleWidth/2, titleY, titleSize, SKYBLUE);
    
    vector<string> options = {"Start Game", "Instructions", "Quit"};
    int startY = 320;
    int spacing = 80;
    
    for(size_t i = 0; i < options.size(); i++) {
        int optionY = startY + i * spacing;
        int textWidth = MeasureText(options[i].c_str(), 30);
        int textX = WINDOW_W/2 - textWidth/2;
        
        bool isSelected = (selectedOption == (int)i);
        Color textColor = isSelected ? YELLOW : WHITE;
        
        if(isSelected) {
            DrawRectangle(textX - 20, optionY - 10, textWidth + 40, 50, Fade(SKYBLUE, 0.3f));
            DrawText(">", textX - 50, optionY, 30, YELLOW);
        }
        
        DrawText(options[i].c_str(), textX, optionY, 30, textColor);
    }
    
    DrawText("Use UP/DOWN arrows and ENTER to select", WINDOW_W/2 - MeasureText("Use UP/DOWN arrows and ENTER to select", 16)/2, 
             WINDOW_H - 80, 16, LIGHTGRAY);
}

void DrawModeSelect(int& selectedMode) {
    ClearBackground(Color{20, 20, 40, 255});
    
    const char* title = "SELECT GAME MODE";
    int titleWidth = MeasureText(title, 40);
    DrawText(title, WINDOW_W/2 - titleWidth/2, 100, 40, SKYBLUE);
    
    vector<string> modes = {"Manual (Player Controlled)", "AI (Automatic Placement)"};
    int startY = 280;
    int spacing = 120;
    
    for(size_t i = 0; i < modes.size(); i++) {
        int modeY = startY + i * spacing;
        int textWidth = MeasureText(modes[i].c_str(), 28);
        int textX = WINDOW_W/2 - textWidth/2;
        
        bool isSelected = (selectedMode == (int)i);
        Color textColor = isSelected ? YELLOW : WHITE;
        
        if(isSelected) {
            DrawRectangle(textX - 30, modeY - 15, textWidth + 60, 60, Fade(SKYBLUE, 0.3f));
            DrawText(">", textX - 60, modeY, 28, YELLOW);
        }
        
        DrawText(modes[i].c_str(), textX, modeY, 28, textColor);
    }
    
    DrawText("Use UP/DOWN arrows and ENTER to select", WINDOW_W/2 - MeasureText("Use UP/DOWN arrows and ENTER to select", 14)/2, 
             WINDOW_H - 60, 14, LIGHTGRAY);
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
        {"ESC", "Return to menu"}
    };
    
    for(size_t i = 0; i < instructions.size(); i++) {
        int y = startY + i * lineHeight;
        DrawText(instructions[i].first.c_str(), 120, y, 20, YELLOW);
        DrawText("-", 280, y, 20, WHITE);
        DrawText(instructions[i].second.c_str(), 310, y, 20, WHITE);
    }
    
    DrawText("OBJECTIVE:", 120, startY + 280, 24, SKYBLUE);
    DrawText("Clear lines by filling rows completely.", 120, startY + 315, 18, LIGHTGRAY);
    DrawText("Game speeds up every 10 lines.", 120, startY + 345, 18, LIGHTGRAY);
    DrawText("Don't let blocks reach the top!", 120, startY + 375, 18, LIGHTGRAY);
    
    DrawText("Press ENTER to return to menu", WINDOW_W/2 - MeasureText("Press ENTER to return to menu", 18)/2, 
             WINDOW_H - 60, 18, YELLOW);
}

void DrawGameScreen(Game& game) {
    ClearBackground(BLACK);

    int boardX = 20, boardY = 20;
    DrawRectangle(boardX-4, boardY-4, BOARD_W*CELL+8, BOARD_H*CELL+8, DARKGRAY);
    DrawRectangle(boardX, boardY, BOARD_W*CELL, BOARD_H*CELL, LIGHTGRAY);

    // Draw board
    for(int r=0;r<BOARD_H;r++){
        for(int c=0;c<BOARD_W;c++){
            int v = game.board.cells[r][c];
            if(v){
                DrawRectangle(boardX + c*CELL, boardY + r*CELL, CELL-2, CELL-2, PALETTE[v]);
            }
        }
    }

    // Draw current piece
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

    // Grid lines
    for(int i=0;i<=BOARD_W;i++) DrawLine(boardX + i*CELL, boardY, boardX + i*CELL, boardY + BOARD_H*CELL, Fade(BLACK,0.12f));
    for(int i=0;i<=BOARD_H;i++) DrawLine(boardX, boardY + i*CELL, boardX + BOARD_W*CELL, boardY + i*CELL, Fade(BLACK,0.12f));

    // Sidebar info
    int sidebarX = boardX + BOARD_W*CELL + 20;
    int sidebarY = boardY;
    
    string modeStr = (game.mode == MANUAL) ? "MANUAL" : "AI";
    DrawText(TextFormat("Mode: %s", modeStr.c_str()), sidebarX, sidebarY, 16, YELLOW);
    DrawText(TextFormat("Score: %d", game.score), sidebarX, sidebarY + 28, 20, WHITE);
    DrawText(TextFormat("Lines: %d", game.lines), sidebarX, sidebarY + 52, 18, WHITE);
    DrawText(TextFormat("Level: %d", game.level), sidebarX, sidebarY + 76, 18, WHITE);

    // Controls info
    DrawText("Controls (Manual):", sidebarX, sidebarY + 130, 12, SKYBLUE);
    DrawText("Arrows: Move/Drop", sidebarX, sidebarY + 150, 10, WHITE);
    DrawText("Up/X: Rotate CW", sidebarX, sidebarY + 165, 10, WHITE);
    DrawText("Z: Rotate CCW", sidebarX, sidebarY + 180, 10, WHITE);
    DrawText("Space: Hard Drop", sidebarX, sidebarY + 195, 10, WHITE);
    DrawText("P: Pause", sidebarX, sidebarY + 210, 10, WHITE);
    DrawText("ESC: Menu", sidebarX, sidebarY + 225, 10, WHITE);

    if(game.paused){
        DrawRectangle(0, WINDOW_H/2 - 40, WINDOW_W, 80, Fade(BLACK, 0.5f));
        DrawText("PAUSED", WINDOW_W/2 - MeasureText("PAUSED", 40)/2, WINDOW_H/2 - 20, 40, YELLOW);
    }

    if(game.gameOver){
        DrawRectangle(0, WINDOW_H/2 - 80, WINDOW_W, 160, Fade(BLACK, 0.7f));
        DrawText("GAME OVER", WINDOW_W/2 - MeasureText("GAME OVER", 50)/2, WINDOW_H/2 - 50, 50, RED);
        DrawText(TextFormat("Score: %d  Lines: %d  Level: %d", game.score, game.lines, game.level), 
                 WINDOW_W/2 - MeasureText("Score", 20)/2 - 80, WINDOW_H/2 + 20, 20, WHITE);
        DrawText("ESC: Menu", WINDOW_W/2 - MeasureText("ESC: Menu", 16)/2, WINDOW_H/2 + 60, 16, LIGHTGRAY);
    }
}

// ==================== MAIN ====================
int main(){
    InitWindow(WINDOW_W, WINDOW_H, "Tetris - Manual & AI Modes");
    SetTargetFPS(60);

    GameState gameState = MAIN_MENU;
    GameMode selectedMode = MANUAL;
    int selectedMenuOption = 0;
    bool showInstructions = false;
    float animTime = 0.0f;

    Game* game = nullptr;

    while(!WindowShouldClose()){
        float dt = GetFrameTime();
        animTime += dt;
        
        // MAIN MENU
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
                        gameState = MODE_SELECT;
                        selectedMode = MANUAL;
                    } else if(selectedMenuOption == 1) {
                        showInstructions = true;
                    } else if(selectedMenuOption == 2) {
                        break;
                    }
                }
            } else {
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
        
        // MODE SELECTION
        if(gameState == MODE_SELECT) {
            if(IsKeyPressed(KEY_UP)) {
                selectedMode = (selectedMode == MANUAL) ? AI : MANUAL;
            }
            if(IsKeyPressed(KEY_DOWN)) {
                selectedMode = (selectedMode == MANUAL) ? AI : MANUAL;
            }
            if(IsKeyPressed(KEY_ENTER)) {
                if(game) delete game;
                game = new Game(selectedMode);
                gameState = PLAYING;
            }
            if(IsKeyPressed(KEY_ESCAPE)) {
                gameState = MAIN_MENU;
                selectedMenuOption = 0;
            }
            
            BeginDrawing();
            DrawModeSelect((int&)selectedMode);
            EndDrawing();
            continue;
        }
        
        // PLAYING
        if(gameState == PLAYING && game) {
            if(IsKeyPressed(KEY_P) && game->mode == MANUAL) {
                game->paused = !game->paused;
            }
            
            if(game->gameOver) {
                if(IsKeyPressed(KEY_ESCAPE)) {
                    gameState = MAIN_MENU;
                    selectedMenuOption = 0;
                    delete game;
                    game = nullptr;
                }
            }
            
            if(game->mode == MANUAL) {
                game->updateManual(dt);
            } else {
                game->updateAI(dt);
            }
            
            BeginDrawing();
            DrawGameScreen(*game);
            EndDrawing();
        }
    }

    if(game) delete game;
    CloseWindow();
    return 0;
}
