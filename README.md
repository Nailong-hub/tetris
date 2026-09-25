# tetris

a repository for algorithm research of tetris

There's only a Cplus version for tetris game now but will be renewed in near future.

# introduction to different versions of tetris game

1. version1: a very basic version of tetris game, use pure Cplus to create a game frame, with most of the ability includes displaying current score/level/line and next shape. 
  1.1. version1.1: add a .txt file to record the highest score in history, and can be displayed.

2. version2: a trained version that can play this game by itself. this model uses Genetic Algorithm(GA) to achieve it. With this Cplus code, you can train your own tetris ai.
>something you should notice
>-  how to train a tetris ai?\
>You can use the file tetris_train.cpp, this is just a simulation data version of tetris_gene.cpp, without any render, only evolution of data displayed. It takes few minutes, but, to some extend, not so visualized.  
>Or, it's a good idea to use tetris_gene.cpp to train your model, then you can see how the process going on. You need to set variation "g_render" and "g_train" for true, but g_render takes time. Speed is lower than train mode.
>-  file you need download\
>If you only want to see how it works rather than experience what the process is, then you can only download file "tetris_gene.cpp", "log.txt" and "gene_progress.txt". Log file provide a record of statistics once when you carry on a train. And Cplus file will find the best trained data from gene_progess.txt.
>- how does it works?\
>I set 4 variations for each of 7 shapes, including linesCleared(double), aggregateHeight(double), holes(double) and bumpiness(double). These are called **FEARURES** in a GA content. And our orientation is to find the best parameter for these 4 * 7 variations. we don't need to find them by our own. Actually, by experiment, I find that 28 variations and double-precision floating-point type are enough for gaining an excellent result. And POP, GENS, ELITE and MUT_RATE can be changed if **OVERFITTING** occurs

- exist problems:
  1. resolution is different within different computers, and frame is displayed differently(become smaller on computers with higher resolution, vice versa).
  2. can not show a choice interface
  3. can not pause when game goes on
  4. the evolution velocity is not quick enough, with still a vast time taken for training from zero to seccess. 
