#include "quadtree.h"

#define QUADRENT_SIZE 10000.0

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
   double x, xMax, xMin;
   double y, yMax, yMin;

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

Node* createNode(double x, double y);

Node* createNode(double x, double y)
{
   Node* node;

   node = malloc(sizeof(Node));

   node->x = x;
   node->y = y;

   node->pilots = malloc(sizeof(LinkedList));
   node->weapons = malloc(sizeof(LinkedList));
   node->planets = malloc(sizeof(LinkedList));
   node->jumps = malloc(sizeof(LinkedList));

   node->nw = NULL;
   node->ne = NULL;
   node->sw = NULL;
   node->se = NULL;

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
   int quadrentSizeDiv;

   cur = top;
   quadrentSizeDiv = 1;

   gfx = outfit_gfx(weapon->outfit);

   while (!added) {
      /* Colides with child bounderies. Place in this node */
      if ((weapon->solid->pos.x - gfx->sw/2 < cur->x && weapon->solid->pos.x + gfx->sw/2 > cur->x) ||
            (weapon->solid->pos.y - gfx->sh/2 < cur->y && weapon->solid->pos.y + gfx->sh/2 > cur->y)) {
         weaponNode = malloc(sizeof(LinkedList));
         weaponNode->u.weapon = weapon;
         weaponNode->next = cur->weapons->next;
         cur->weapons->next = weaponNode;
         added = 1;
      }
      else {
         if (weapon->solid->pos.x > cur->x) {
            if (weapon->solid->pos.y > cur->y) {
               if (cur->ne == NULL)
                  cur->ne = createNode(cur->x + QUADRENT_SIZE/quadrentSizeDiv, cur->y + QUADRENT_SIZE/quadrentSizeDiv);
               cur = cur->ne;
            }
            else {
               if (cur->se == NULL)
                  cur->se = createNode(cur->x + QUADRENT_SIZE/quadrentSizeDiv, cur->y - QUADRENT_SIZE/quadrentSizeDiv);
               cur = cur->se;
            }
         }
         else if (weapon->solid->pos.y > cur->y) {
            if (cur->nw == NULL)
               cur->nw = createNode(cur->x - QUADRENT_SIZE/quadrentSizeDiv, cur->y + QUADRENT_SIZE/quadrentSizeDiv);
            cur = cur->nw;
         }
         else {
            if (cur->sw == NULL)
               cur->sw = createNode(cur->x - QUADRENT_SIZE/quadrentSizeDiv, cur->y - QUADRENT_SIZE/quadrentSizeDiv);
            cur = cur->sw;
         }
         quadrentSizeDiv *= 2;
      }
   }

   return added;
}

int quadtree_removeWeapon(Weapon* weapon)
{
   
}
