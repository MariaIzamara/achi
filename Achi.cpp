#include "Achi.h"
#include "ui_Achi.h"

#include <QMessageBox>
#include <QSignalMapper>

#include <QDebug>

Hole::State playerToState(Achi::Player player) {
    return player == Achi::RedPlayer ? Hole::RedState : Hole::BlueState;
}

Achi::Player stateToPlayer(Hole::State state) {
    switch (state) {
        case Hole::RedState:
            return Achi::RedPlayer;
        case Hole::BlueState:
            return Achi::BluePlayer;
        default:
            Q_UNREACHABLE();
    }
}

bool isSelectable(Hole* hole) {
    return hole != nullptr &&
            (hole->state() == Hole::EmptyState ||
             hole->state() == Hole::SelectableState);
}

Achi::Achi(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::Achi),
      m_player(Achi::RedPlayer),
      m_phase(Achi::DropPhase),
      m_dropCount(0),
      m_selected(nullptr) {

    ui->setupUi(this);

    QObject::connect(ui->actionNew, SIGNAL(triggered(bool)), this, SLOT(reset()));
    QObject::connect(ui->actionQuit, SIGNAL(triggered(bool)), qApp, SLOT(quit()));
    QObject::connect(ui->actionAbout, SIGNAL(triggered(bool)), this, SLOT(showAbout()));
    QObject::connect(this, SIGNAL(gameOver(Player)), this, SLOT(showGameOver(Player)));
    QObject::connect(this, SIGNAL(gameOver(Player)), this, SLOT(reset()));

    // Load the pieces.
    QSignalMapper* map = new QSignalMapper(this);
    for (int id = 0; id < 9; ++id) {
        int r = id / 3;
        int c = id % 3;

        Hole* hole = this->findChild<Hole*>(QString("hole%1%2").arg(r).arg(c));
        Q_ASSERT(hole != nullptr);

        m_holes[id] = hole;
        map->setMapping(hole, id);
        QObject::connect(hole, SIGNAL(clicked()), map, SLOT(map()));
    }
    QObject::connect(map, SIGNAL(mapped(int)), this, SLOT(play(int)));

    this->updateStatusBar();

    // Compact the layout of the widgets.
    this->adjustSize();

    // Set a fixed window size.
    this->setFixedSize(this->size());
}

Achi::~Achi() {
    delete ui;
}

void Achi::play(int id) {
    Hole* hole = m_holes[id];

    switch (m_phase) {
        case Achi::DropPhase:
            drop(hole);
            break;
        case Achi::MovePhase:
            move(hole);
            break;
        default:
            Q_UNREACHABLE();
    }
}

void Achi::drop(Hole* hole) {
    if (hole->state() == Hole::EmptyState) {
        hole->setState(playerToState(m_player));

        if (isGameOver(hole))
            emit gameOver(m_player);
        else {
            ++m_dropCount;
            if (m_dropCount == 6)
                m_phase = Achi::MovePhase;

            this->switchPlayer();
        }
    }
}

Hole* Achi::holeAt(int row, int col) const {
    if (row >= 0 && row < 3 && col >= 0 && col < 3)
        return m_holes[row * 3 + col];
    else
        return 0;
}

bool Achi::isGameOver(Hole* hole) {
    Achi::Player player = stateToPlayer(hole->state());
    return this->checkRow(player, hole->col()) || this->checkCol(player, hole->row()) || this->checkDiagonal(player);
}

bool Achi::checkRow(Player player, int col) {
    Hole::State state = playerToState(player);
    for (int row = 0; row < 3; row++) {
        Hole* tmp = this->holeAt(row, col);
        Q_ASSERT(tmp != 0);

        switch (tmp->state()) {
            case Hole::RedState:
            case Hole::BlueState:
                if (state != tmp->state())
                    return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

bool Achi::checkCol(Player player, int row) {
    Hole::State state = playerToState(player);
    for (int c = 0; c < 3; c++) {
        Hole* tmp = this->holeAt(row, c);
        Q_ASSERT(tmp != 0);

        switch (tmp->state()) {
            case Hole::RedState:
            case Hole::BlueState:
                if (state != tmp->state())
                    return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

bool checkState(Hole::State state, Hole* tmp) {
    Q_ASSERT(tmp != 0);

    switch (tmp->state()) {
        case Hole::RedState:
        case Hole::BlueState:
            if (state != tmp->state())
                return false;
            break;
        default:
            return false;
    }
    return true;
}

bool Achi::checkDiagonal(Player player) {
    Hole::State state = playerToState(player);

    bool diagonal_right = checkState(state, this->holeAt(0, 0)) && checkState(state, this->holeAt(1, 1)) && checkState(state, this->holeAt(2, 2));
    bool diagonal_left = checkState(state, this->holeAt(0, 2)) && checkState(state, this->holeAt(1, 1)) && checkState(state, this->holeAt(2, 0));

    return diagonal_right || diagonal_left;
}

void Achi::switchPlayer() {
    m_player = m_player == Achi::RedPlayer ? Achi::BluePlayer : Achi::RedPlayer;
    this->updateStatusBar();
}

void Achi::move(Hole* hole) {
    QPair<Hole*,Hole*>* movement = nullptr;

    if (hole->state() == Hole::SelectableState) {
        Q_ASSERT(m_selected != 0);
        movement = new QPair<Hole*,Hole*>(m_selected, hole);
    } else {
        if (hole->state() == playerToState(m_player)) {
            QList<Hole*> selectable = this->findSelectable(hole);
            if (selectable.count() == 1) {
                movement = new QPair<Hole*,Hole*>(hole, selectable.at(0));
            } else if (selectable.count() > 1) {
                this->clearSelectable();
                foreach (Hole* tmp, selectable)
                    tmp->setState(Hole::SelectableState);

                m_selected = hole;
            }
        }
    }

    if (movement != nullptr) {
        this->clearSelectable();
        m_selected = 0;

        Q_ASSERT(movement->first->state() == playerToState(m_player));
        Q_ASSERT(movement->second->state() == Hole::EmptyState);

        movement->first->setState(Hole::EmptyState);
        movement->second->setState(playerToState(m_player));

        if (isGameOver(movement->second))
            emit gameOver(m_player);
        else
            this->switchPlayer();

        delete movement;
    }
}

QList<Hole*> Achi::findSelectable(Hole* hole) {
    QList<Hole*> list;

    // qDebug() << QString("hole: (%1, %2)").arg(hole->row()).arg(hole->col());

    Hole* left = this->holeAt(hole->row() - 1, hole->col());
    if (isSelectable(left))
        list << left;

    Hole* up = this->holeAt(hole->row(), hole->col() - 1);
    if (isSelectable(up))
        list << up;

    Hole* right = this->holeAt(hole->row() + 1, hole->col());
    if (isSelectable(right))
        list << right;

    Hole* bottom = this->holeAt(hole->row(), hole->col() + 1);
    if (isSelectable(bottom))
        list << bottom;

    int hole_value = hole->row() * 3 + hole->col();

    if (hole_value%2 == 0) {
        Hole* diagonal_left_up = this->holeAt(hole->row() - 1, hole->col() - 1);
        if (isSelectable(diagonal_left_up))
            list << diagonal_left_up;

        Hole* diagonal_left_right = this->holeAt(hole->row() + 1, hole->col() - 1);
        if (isSelectable(diagonal_left_right))
            list << diagonal_left_right;

        Hole* diagonal_right_up = this->holeAt(hole->row() - 1, hole->col() + 1);
        if (isSelectable(diagonal_right_up))
            list << diagonal_right_up;

        Hole* diagonal_right_bottom = this->holeAt(hole->row() + 1, hole->col() + 1);
        if (isSelectable(diagonal_right_bottom))
            list << diagonal_right_bottom;
    }

    return list;
}

void Achi::clearSelectable() {
    for (int id = 0; id < 9; id++) {
        Hole* hole = m_holes[id];
        if (hole->state() == Hole::SelectableState)
            hole->setState(Hole::EmptyState);
    }
}

void Achi::reset() {
    for (int id = 0; id < 9; ++id) {
        Hole* hole = m_holes[id];
        Q_ASSERT(hole != nullptr);

        m_holes[id]->reset();
    }

    m_player = Achi::RedPlayer;
    m_phase = Achi::DropPhase;
    m_dropCount = 0;
    m_selected = nullptr;

    this->updateStatusBar();
}

void Achi::showAbout() {
    QMessageBox::information(this, tr("Sobre"), tr("Desenvolvedores:\n\n"
                                                   "- Felipe Martins Lemos de Morais\n  felipemartinsldemorais@gmail.com\n"
                                                   "- Maria Izamara Clara da Silva Coutinho\n  mariaizamaracoutinho@gmail.com"));
}

void Achi::showGameOver(Player player) {
    switch (player) {
        case Achi::RedPlayer:
            QMessageBox::information(this, tr("Vencedor"), tr("Parabéns, o jogador vermelho venceu."));
            break;
        case Achi::BluePlayer:
            QMessageBox::information(this, tr("Vencedor"), tr("Parabéns, o jogador azul venceu."));
            break;
        default:
            Q_UNREACHABLE();
    }
}

void Achi::updateStatusBar() {
    QString player(m_player == Achi::RedPlayer ? "vermelho" : "azul");
    QString phase(m_phase == Achi::DropPhase ? "colocar" : "mover");

    ui->statusbar->showMessage(tr("Fase de %1: vez do jogador %2").arg(phase).arg(player));
}
