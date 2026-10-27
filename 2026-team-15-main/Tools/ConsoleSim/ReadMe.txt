Console demo for the coffee simulation (M1G06)
==============================================
Runs the simulation in a terminal so you can see orders, stock, step grading and the session report.
It is a separate desktop program. It does not use the Android build or the ECS code.

Build (from the repo root):
    cmake -S Tools/ConsoleSim -B build-sim
    cmake --build build-sim

Run:
    ./build-sim/console_sim                       (Linux / macOS / WSL)
    build-sim\Debug\console_sim.exe               (Visual Studio generator on Windows)

Options:
    --scenario Tutorial|Normal|Rushhour           which scenario from Assets/Json/Scenarios.json
    --interactive                                 you are the barista (type "help")
    --speed 5                                     slow the bot down (5 simulated seconds per real second)
    --seed 99                                     different customers (0 = random every run)
    --assets <folder>                             where the JSON files are

Tuning without recompiling: edit Assets/Json/Simulation.json and run again.


GUIDED MODE (you type every amount, 60 second shift)
----------------------------------------------------
    build-sim\Debug\console_sim.exe --guided            (Windows)
    ./build-sim/console_sim --guided                    (Linux/Mac)

    --duration S    shift length in seconds (default 60)
    --patience S    customer patience in seconds (default: PatienceSec in Assets/Json/Simulation.json)

While playing:
    - Type a number for each "Add ..." prompt. A wrong amount is wasted and you must redo the step.
    - When a shelf runs low you are asked to type:  Refill <ingredient>   (e.g. Refill Condensed Milk)
      The refill amount comes from Simulation.json (Inventory -> RefillAmount).
    - Type "quit" to stop early. The session report prints at the end.
Everything (stock, patience, refill amounts, low-stock level) is tuned in Assets/Json/Simulation.json, no recompiling.
