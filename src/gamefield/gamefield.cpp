#include "gamefield.h"

#include <QKeyEvent>
#include <QPainter>
#include <QMessageBox>

GameField::GameField(QWidget *parent, int colorSwitchLimit)
: Table(parent), colorSwitchCount_(colorSwitchLimit), timer_(this) {
    connect(&timer_, &QTimer::timeout, this, &GameField::FigureDrop);
}

void GameField::StartNewGame() {
    setFocus();
    ResetCellsColor();
    nextFigure_ = figureGenerator_();
    NextFigure();
    score_ = 0;
    emit ScoreUpdated(score_);
    timer_.start(500);
}

void GameField::StopGame() {
    ResetCellsColor();
    timer_.stop();
    score_ = 0;
    emit GameOver();
}

void GameField::FigureDrop() {
    DrawFigure(currentFigure_);
    repaint();
    if (IsOutOfBounds(currentFigure_, 0, 1)) {
        if (currentFigure_.GetTop() + dy_)
            NextFigure();
        else {
            QMessageBox::critical(
                (QWidget*)this->parent(), "Game over",
                "Your block stack has grown too high."
            );
            StopGame();
        }
        return;
    }
    EraseFigure(currentFigure_);
    ++dy_;
}

void GameField::NextFigure() {
    dy_ = 0, dx_ = 0, colorSwitches_ = 0;
    emit FigureColorSwitched(colorSwitchCount_);
    currentFigure_ = std::exchange(nextFigure_, figureGenerator_());
    emit NextFigureUpdated(nextFigure_);
    if (uint score = CountScore()) {
        repaint();
        score_ += score;
        emit ScoreUpdated(score_);
    }
}

void GameField::keyPressEvent(QKeyEvent *event) {
    switch(event->key()) {
        case Qt::Key_Up:
            RedrawFigure(
                !IsOutOfBounds(currentFigure_.Rotated()),
                [this](){ currentFigure_.Rotate(); }
            );
            break;
        case Qt::Key_Left:
            RedrawFigure(
                !IsOutOfBounds(currentFigure_, -1),
                [this](){ --dx_; }
            );
            break;
        case Qt::Key_Down:
            RedrawFigure(
                !IsOutOfBounds(currentFigure_, 0, 1),
                [this](){ ++dy_; }
            );
            break;
        case Qt::Key_Right:
            RedrawFigure(
                !IsOutOfBounds(currentFigure_, 1),
                [this](){ ++dx_; }
            );
            break;
        case Qt::Key_Space:
            RedrawFigure(
                colorSwitches_ < colorSwitchCount_,
                [this](){
                    ++colorSwitches_;
                    emit FigureColorSwitched(colorSwitchCount_ - colorSwitches_);
                    currentFigure_.SetColors(
                        figureGenerator_.GenerateColors()
                    );
                }
            );
            break;
    }
}

void GameField::RedrawFigure(bool condition, auto&& function) {
    if (condition) {
        EraseFigure(currentFigure_);
        std::forward<decltype(function)>(function)();
        DrawFigure(currentFigure_);
        repaint();
    }
}

void GameField::DrawFigure(const Figure& figure) {
    auto colors = figure.GetColors();
    for (auto&& cell : figure.GetGeometry())
        cellsColors_[cell[0] + dy_][cell[1] + dx_] = colors[cell[2]];
}

void GameField::EraseFigure(const Figure& figure) {
    for (auto&& cell : figure.GetGeometry())
        cellsColors_[cell[0] + dy_][cell[1] + dx_] = kCellDefaultColor;
}

bool GameField::IsOutOfBounds(const Figure& figure, int dx, int dy) {
    bool result = static_cast<int>(figure.GetLeft()) + dx_ + dx < 0
                  || static_cast<int>(figure.GetRight())+ dx_ + dx >= columnsCount_
                  || static_cast<int>(figure.GetBottom()) + dy_ + dy >= rowsCount_
                  || std::ranges::any_of(
        std::ranges::views::filter(figure.GetGeometry(), [this, dx, dy](auto&& cell){
            return std::ranges::all_of(
                currentFigure_.GetGeometry(),
                [this, cell = std::forward<decltype(cell)>(cell), dx, dy](auto&& current){
                    return std::forward<decltype(current)>(current)[0] + dy_ != cell[0] + dy_ + dy
                           || std::forward<decltype(current)>(current)[1] + dx_ != cell[1] + dx_ + dx;
                }
            );
        }), [this, dx, dy](auto&& cell) {
            return cellsColors_[std::forward<decltype(cell)>(cell)[0] + dy_ + dy]
                   [std::forward<decltype(cell)>(cell)[1] + dx_ + dx] != kCellDefaultColor;
        }
    );
    return result;
}

uint GameField::CountScore() noexcept {
    constexpr uint minSeq = 3;
    uint vSeq = minSeq, hSeq = minSeq, score = 0;
    while (vSeq > 1 && hSeq > 1) {
        for (uint i = 1; i < rowsCount_; ++i) {
            hSeq = 1;
            for (uint j = 1; j < columnsCount_; ++j) {
                if (cellsColors_[i][j] != kCellDefaultColor && cellsColors_[i][j] == cellsColors_[i][j - 1])
                    ++hSeq;
                else {
                    if (hSeq >= minSeq) {
                        score += hSeq;
                        for (uint k = i; k > 0; --k)
                            for (uint l = 1; l <= hSeq; ++l)
                                cellsColors_[k][j - l] = cellsColors_[k - 1][j - l];
                    }
                    hSeq = 1;
                }
            }
            if (hSeq >= minSeq) {
                score += hSeq;
                for (uint k = i; k > 0; --k)
                    for (uint l = 1; l <= hSeq; ++l)
                        cellsColors_[k][columnsCount_ - l] = cellsColors_[k - 1][columnsCount_ - l];
            }
        }
        for (uint j = 0; j < columnsCount_; ++j) {
            vSeq = 1;
            for (uint i = 1; i < rowsCount_; ++i) {
                if (cellsColors_[i][j] != kCellDefaultColor && cellsColors_[i][j] == cellsColors_[i - 1][j])
                    ++vSeq;
                else {
                    if (vSeq >= minSeq) {
                        score += vSeq;
                        for (uint k = 1; k < i - vSeq; ++k)
                            cellsColors_[i - k][j] = cellsColors_[i - vSeq - k][j];
                        for (uint k = i - vSeq; k <= i; ++k)
                            cellsColors_[i - k][j] = kCellDefaultColor;
                    }
                    vSeq = 1;
                }
            }
            if (vSeq >= minSeq) {
                score += vSeq;
                for (uint k = 1; k <= rowsCount_ - vSeq; ++k)
                    cellsColors_[rowsCount_ - k][j] = cellsColors_[rowsCount_ - vSeq - k][j];
                for (uint k = rowsCount_ - vSeq; k <= rowsCount_; ++k)
                    cellsColors_[rowsCount_ - k][j] = kCellDefaultColor;
            }
        }
    }
    return score;
}
