/*
 * signals.h - Deklaracje obsługi sygnałów
 *
 * should_exit - Flaga globalna ustawiana przez handler sygnału
 * Funkcje do konfiguracji handlerów i sprzątania przy wyjściu.
 */

#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

extern volatile sig_atomic_t should_exit;

void setup_signal_handlers();
void cleanup_on_exit();

#endif
