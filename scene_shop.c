#include "scene.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"

static void ShopDraw(Scene *scene, void *ctx)
{
    (void)scene;
    AppContext *app = (AppContext *)ctx;
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    Vector2 mouse = GetMousePosition();

    BeginDrawing();
    ClearBackground((Color){ 20, 22, 28, 255 });

    UIDrawCenteredText("Crystal Shop", screenW / 2, 20, 36, WHITE);

    // Crystal balance
    DrawText(TextFormat("Crystals: %d", app->profile.crystals), screenW - 220, 25, 24, (Color){180,100,255,255});

    // Shop categories (data-driven)
    const char *catNames[SHOP_CAT_COUNT] = { "Stat Boosts", "Tower Unlocks", "Abilities" };

    int shopY = 65;
    for (int cat = 0; cat < SHOP_CAT_COUNT; cat++) {
        DrawText(catNames[cat], 30, shopY, 20, GOLD);
        shopY += 26;

        for (int itemID = 0; itemID < SHOP_ITEM_COUNT; itemID++) {
            if ((int)SHOP_ITEMS[itemID].category != cat) continue;
            const ShopItemConfig *si = &SHOP_ITEMS[itemID];
            bool purchased = app->profile.shopPurchased[itemID];
            bool canBuy = ShopCanPurchase(&app->profile, (ShopItemID)itemID);

            int itemX = 40;
            int itemW = screenW - 80;
            int itemH = 32;
            Rectangle itemRect = { (float)itemX, (float)shopY, (float)itemW, (float)itemH };
            bool hover = CheckCollisionPointRec(mouse, itemRect);

            Color bg = purchased ? (Color){30,50,30,200} :
                       canBuy && hover ? (Color){50,60,80,255} :
                       canBuy ? (Color){35,40,50,200} :
                       (Color){30,30,35,200};
            DrawRectangleRec(itemRect, bg);
            DrawRectangleLinesEx(itemRect, 1, (Color){60,60,70,200});

            Color nameCol = purchased ? (Color){100,180,100,255} : canBuy ? WHITE : (Color){100,100,100,255};
            DrawText(si->name, itemX + 8, shopY + 4, 16, nameCol);
            DrawText(si->description, itemX + 200, shopY + 4, 14, (Color){180,180,180,255});

            if (purchased) {
                DrawText("OWNED", itemX + itemW - 70, shopY + 8, 14, (Color){100,180,100,255});
            } else {
                Color costCol = canBuy ? (Color){180,100,255,255} : (Color){100,60,60,255};
                DrawText(TextFormat("%d", si->cost), itemX + itemW - 60, shopY + 8, 14, costCol);
            }

            // Buy on click
            if (!purchased && canBuy && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ShopPurchase(&app->profile, (ShopItemID)itemID);
                PlayerProfileSave(&app->profile, "profile.fdsave");
            }

            shopY += itemH + 2;
        }
        shopY += 8;
    }

    // Continue button
    int cBtnW = 200, cBtnH = 45;
    int cBtnX = (screenW - cBtnW) / 2;
    int cBtnY = screenH - 110;
    UIButtonResult contBtn = UIButton(cBtnX, cBtnY, cBtnW, cBtnH, "Continue", 24, &UI_STYLE_PRIMARY);

    int sBkY = cBtnY + cBtnH + 10;
    UIButtonResult backBtn = UIButton(cBtnX, sBkY, cBtnW, 40, "Back", 22, &UI_STYLE_DANGER);

    EndDrawing();

    if (contBtn.clicked) {
        RunModifiersInit(&app->runMods, &app->profile);
        app->perkSeed = (unsigned int)(GetTime() * 1000.0);
        PerkSelectRandom(app->perkOffered, app->perkSeed);
        SceneManagerTransition(app->sceneManager, SCENE_PERK_SELECT);
    }
    if (backBtn.clicked || IsKeyPressed(KEY_ESCAPE)) {
        SceneManagerTransition(app->sceneManager, SCENE_DIFFICULTY_SELECT);
    }
}

Scene SceneShopCreate(void)
{
    return (Scene){
        .name = "Shop",
        .draw = ShopDraw,
    };
}
