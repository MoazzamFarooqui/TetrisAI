// tetris.cpp
// Compile example (Linux/macOS):
// g++ -std=c++17 tetris.cpp -o tetris -lraylib -lpthread -ldl -lm
//
// Compact Tetris with an AI that automatically places pieces.
// Uses raylib for graphics (no assets required).

#include "raylib.h"
#include <array>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <limits>
#include <iostream>
#include <string>
#include <climits>

// --- Config ---
const int BOARD_W = 10;
const int BOARD_H = 20;
const int CELL = 24;
const int WINDOW_W = BOARD_W * CELL + 220;
const int WINDOW_H = BOARD_H * CELL;
const float DROP_INTERVAL = 0.5f;
const int BAG_SIZE = 7;

// Heuristic weights
const double W_LINES = 0.760666;
const double W_HOLE = -0.35663;
const double W_HEIGHT = -0.510066;
const double W_BUMPY = -0.184483;

// Colors
const Color COLORS[8] = {
    BLACK,
    RED, ORANGE, GOLD, GREEN, BLUE, PURPLE, SKYBLUE
};

// --- Tetromino definitions ---
using Matrix4 = std::array<std::array<int,4>,4>;
struct Tetromino {
    std::vector<Matrix4> states;
    int colorId;
};

Matrix4 rotate90(const Matrix4 &m) {
    Matrix4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[i][j] = m[3 - j][i];
    return r;
}

std::vector<Tetromino> makeTetrominoes() {
    std::vector<Tetromino> T(7);
    // I
    Matrix4 I0 = {{
        {0,0,0,0},
        {1,1,1,1},
        {0,0,0,0},
        {0,0,0,0}
    }};
    T[0].states = {I0, rotate90(I0)};
    T[0].colorId = 7;

    // J
    Matrix4 J0 = {{
        {1,0,0,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }};
    for (int i=0;i<4;i++) {
        if (i==0) T[1].states.push_back(J0);
        else T[1].states.push_back(rotate90(T[1].states.back()));
    }
    T[1].colorId = 6;

    // L
    Matrix4 L0 = {{
        {0,0,1,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }};
    for (int i=0;i<4;i++) {
        if (i==0) T[2].states.push_back(L0);
        else T[2].states.push_back(rotate90(T[2].states.back()));
    }
    T[2].colorId = 3;

    // O
    Matrix4 O0 = {{
        {0,1,1,0},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }};
    T[3].states = {O0};
    T[3].colorId = 4;

    // S
    Matrix4 S0 = {{
        {0,1,1,0},
        {1,1,0,0},
        {0,0,0,0},
        {0,0,0,0}
    }};
    T[4].states = {S0, rotate90(S0)};
    T[4].colorId = 2;

    // T
    Matrix4 T0m = {{
        {0,1,0,0},
        {1,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }};
    for (int i=0;i<4;i++) {
        if (i==0) T[5].states.push_back(T0m);
        else T[5].states.push_back(rotate90(T[5].states.back()));
    }
    T[5].colorId = 5;

    // Z
    Matrix4 Z0 = {{
        {1,1,0,0},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }};
    T[6].states = {Z0, rotate90(Z0)};
    T[6].colorId = 1;

    return T;
}

// --- Board representation ---
struct Board {
    std::array<std::array<int, BOARD_W>, BOARD_H> cells{};

    Board() {
        for (auto &row : cells) row.fill(0);
    }

    bool inBounds(int r, int c) const {
        return r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W;
    }

    bool collides(const Matrix4 &shape, int topR, int leftC) const {
        for (int i=0;i<4;i++) for (int j=0;j<4;j++) {
            if (!shape[i][j]) continue;
            int r = topR + i;
            int c = leftC + j;
            if (c < 0 || c >= BOARD_W || r >= BOARD_H) return true;
            if (r >= 0 && cells[r][c] != 0) return true;
        }
        return false;
    }

    void placePiece(const Matrix4 &shape, int topR, int leftC, int colorId) {
        for (int i=0;i<4;i++) for (int j=0;j<4;j++) {
            if (shape[i][j]) {
                int r = topR + i;
                int c = leftC + j;
                if (r >= 0 && r < BOARD_H && c >= 0 && c < BOARD_W)
                    cells[r][c] = colorId;
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

// --- AI decision ---
struct MoveDecision {
    int rotationIndex;
    int leftC;
    double score;
    int lines;
};

MoveDecision findBestMove(const Board &board, const Tetromino &piece) {
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

// --- Random bag ---
struct Bag {
    std::vector<int> bag;
    std::mt19937 rng;
    Bag() {
        rng.seed(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        refill();
    }
    void refill() {
        bag.clear();
        for (int i=0;i<7;i++) bag.push_back(i);
        std::shuffle(bag.begin(), bag.end(), rng);
    }
    int next() {
        if (bag.empty()) refill();
        int t = bag.back();
        bag.pop_back();
        return t;
    }
};

// --- Rendering ---
void DrawCell(int x, int y, int colorId) {
    DrawRectangle(x, y, CELL - 1, CELL - 1, COLORS[colorId]);
    DrawRectangleLines(x, y, CELL - 1, CELL - 1, DARKGRAY);
}

void DrawBoard(const Board &board, int offsetX, int offsetY) {
    DrawRectangle(offsetX - 2, offsetY - 2, BOARD_W*CELL + 4, BOARD_H*CELL + 4, GRAY);
    for (int r = 0; r < BOARD_H; ++r)
        for (int c = 0; c < BOARD_W; ++c) {
            int id = board.cells[r][c];
            Color col = (id >= 0 && id < 8) ? COLORS[id] : LIGHTGRAY;
            DrawRectangle(offsetX + c*CELL, offsetY + r*CELL, CELL - 1, CELL - 1, col);
            DrawRectangleLines(offsetX + c*CELL, offsetY + r*CELL, CELL - 1, CELL - 1, DARKGRAY);
        }
}

int main() {
    InitWindow(WINDOW_W, WINDOW_H, "Tetris AI (Heuristic) - raylib");
    SetTargetFPS(60);

    auto tetrominoes = makeTetrominoes();
    Bag bag;
    Board board;

    int currentType = bag.next();
    int nextType = bag.next();
    bool gameOver = false;
    int score = 0;
    int totalLines = 0;
    double aiTimer = 0.0;
    double aiCooldown = 1.08; // chaged

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (!gameOver) {
            aiTimer += dt;
            if (aiTimer >= aiCooldown) {
                aiTimer = 0.0;
                MoveDecision move = findBestMove(board, tetrominoes[currentType]);
                if (move.score < -1e8) gameOver = true;
                else {
                    Matrix4 shape = tetrominoes[currentType].states[move.rotationIndex];
                    int top = board.dropPosition(shape, move.leftC);
                    if (top == INT_MIN) gameOver = true;
                    else {
                        board.placePiece(shape, top, move.leftC, tetrominoes[currentType].colorId);
                        int lines = board.clearLines();
                        totalLines += lines;
                        if (lines > 0) score += 100 * (1 << (lines - 1));
                        currentType = nextType;
                        nextType = bag.next();
                        Matrix4 spawn = tetrominoes[currentType].states[0];
                        if (board.collides(spawn, -4, BOARD_W/2 - 2))
                            gameOver = true;
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Tetris AI (heuristic)", BOARD_W * CELL + 10, 10, 14, BLACK);
        std::string scoreText = "Score: " + std::to_string(score);
        std::string linesText = "Lines: " + std::to_string(totalLines);
        DrawText(scoreText.c_str(), BOARD_W * CELL + 10, 40, 12, BLACK);
        DrawText(linesText.c_str(), BOARD_W * CELL + 10, 60, 12, BLACK);

        DrawBoard(board, 0, 0);

        DrawText("Next:", BOARD_W * CELL + 10, 100, 12, BLACK);
        const Tetromino &nxt = tetrominoes[nextType];
        for (int i=0;i<4;++i) for (int j=0;j<4;++j)
            if (nxt.states[0][i][j])
                DrawCell(BOARD_W*CELL + 40 + j*CELL/2, 130 + i*CELL/2, nxt.colorId);

        if (gameOver) {
            DrawRectangle(20, WINDOW_H/2 - 40, BOARD_W*CELL - 40, 80, Fade(BLACK, 0.6f));
            DrawText("GAME OVER", BOARD_W*CELL/2 - 60, WINDOW_H/2 - 10, 20, RAYWHITE);
            std::string finalScore = "Final Score: " + std::to_string(score);
            DrawText(finalScore.c_str(), BOARD_W*CELL/2 - 70, WINDOW_H/2 + 16, 14, RAYWHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
