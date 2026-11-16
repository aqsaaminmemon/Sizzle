SIZZLE - COZY CAFE  (Step 4: a real game loop)

BUILD (PowerShell, in this folder; keep assets\ and sounds\ next to main.cpp / Sizzle.exe):
  g++ -std=c++17 main.cpp -o Sizzle.exe -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio
  .\Sizzle.exe
No sound libs? Set  #define ENABLE_AUDIO 0  at the top of main.cpp and drop -lsfml-audio.
If Sizzle.exe says a DLL is missing, copy sfml-*-3.dll and openal32.dll from SFML's bin folder next to it.

HOW TO PLAY
Customers appear on the order cards (left). Cook, assemble, then DRAG each finished dish from its plate onto the matching order card.
  Burger: bottom bun -> ketchup (S skips) -> COOKED patty (raw patty goes on the stove first) -> lettuce -> tomato -> cheese -> top bun  (first plate)
  Pizza : dough -> sauce -> cheese -> pepperoni on the 2nd plate, drag it into the oven, drag it back when READY
  Fries : potatoes into a fryer basket, drag to the fries plate when READY
  Drink : click a glass on the machine, drag it to the drink plate when READY
Burnt food: click it to throw it away. Customers who run out of patience are strikes (3 = level failed).
Serve all orders before the timer ends. Stars depend on strikes. Spend coins in the shop after each level.
Keys: Esc / P = pause, S = skip ketchup, R = clear kitchen.

TUNING (top of main.cpp): levelGoal(), levelTime(), spawnInterval(), upCost(), pattyCook()/fryCook()/pizzaCook(), dishValue[].
