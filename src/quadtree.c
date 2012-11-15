#include "quadtree.h"

typedef struct Node_ Node;
typedef struct LinkedList_ LinkedList;

struct LinkedList_
{
   union Objects
   {
      Pilot* pilot;
      Weapon* weapon;
      Planet* planet;
      JumpPoint* jump;
   } u;

   LinkedList* next;
};

struct Node_
{
   /* bounds */
   int x;
   int y;

   /* contained objects */
   LinkedList* pilots;
   LinkedList* weapons;
   LinkedList* planets;
   LinkedList* jumps;

   /* children */
   Node* nw;
   Node* ne;
   Node* sw;
   Node* se;
};

Node* top;

Node* createNode(int x, int y);

Node* createNode(int x, int y)
{
   Node* node;

   node = malloc(sizeof(Node));

   node->x = x;
   node->y = y;

   node->pilots = malloc(sizeof(LinkedList));
   node->weapons = malloc(sizeof(LinkedList));
   node->planets = malloc(sizeof(LinkedList));
   node->jumps = malloc(sizeof(LinkedList));

   return node;
}

int quadtree_create()
{
   top = createNode(0,0);
   return 1;
}

int quadtree_clean()
{
   /*TODO*/
   return 1;
}

int quadtree_reset()
{
   quadtree_clean();
   return quadtree_create();
}

int quadtree_addWeapon(Weapon* weapon)
{
   Node* cur;
   glTexture* gfx;
   LinkedList* weaponNode;
   int added = 0;

   cur = top;

   gfx = outfit_gfx(weapon->outfit);

   while (!added) {
      /* Colides with bounderies. Place in this node */
      if (!(weapon->solid->pos.x - gfx->sw/2 < cur->x || weapon->solid->pos.x + gfx->sw/2 > cur->x ||
            weapon->solid->pos.y - gfx->sh/2 < cur->y || weapon->solid->pos.y + gfx->sh/2 > cur->y)) {
         weaponNode = malloc(sizeof(LinkedList));
         weaponNode->u.weapon = weapon;
         weaponNode->next = cur->weapons->next;
         cur->weapons->next = weaponNode;
         added = 1;
      }
      else if (weapon->solid->pos.x > cur->x) {
         if (weapon->solid->pos.y > cur->y)
            cur = cur->ne;
         else
            cur = cur->se;
      }
      else if (weapon->solid->pos.y > cur->y)
         cur = cur->nw;
      else
         cur = cur->sw;
   }

   return added;
}

int quadtree_removeWeapon(Weapon* weapon)
{
   
}
