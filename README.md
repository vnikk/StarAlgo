# StarAlgo
Monte Carlo Tree Search Considering Durations library. It provides a useful algorithm for heuristics and control of your army squads. It was developed mainly to be integrated with (UAlbertaBot)[https://github.com/davechurchill/ualbertabot], but it is possible to plug to other architectures as well but with some additional effort.

How to setup the library:

0. Add library file to project.
1. Include headers.
2. Make several bot squads (the more the better).
3. Create search object.
4. Distribute units equally among the squads.
5. Call search on object (#3) and get result.
6. Assign result back to squads.


MCTSCD assigns units to distinct groups by their type and region, so if your squads share the same approach, the result will be more accurate for the squad (otherwise the prevailing group will be used).

You can use the patches/\* files to integrate with UAlberta.
