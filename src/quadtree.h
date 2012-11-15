#ifndef QUADTREE_H
#  define QUADTREE_H

#include "pilot.h"
#include "weapon.h"
#include "space.h"

int quadtree_create(void);
int quadtree_clean(void);
int quadtree_reset(void);

int quadtree_addPilot(Pilot* pilot);
int quadtree_addWeapon(Weapon* weapon);
int quadtree_addPlanet(Planet* planet);
int quadtree_addJump(JumpPoint* jump);

#endif
