#ifndef NFC_CARDGAME_BASE_GEOMETRY_H
#define NFC_CARDGAME_BASE_GEOMETRY_H

#ifndef BASE_NAV_BLOCKER_BACK_OFFSET_BOTTOM
#define BASE_NAV_BLOCKER_BACK_OFFSET_BOTTOM 48.0f
#endif

#ifndef BASE_NAV_BLOCKER_BACK_OFFSET_TOP
#define BASE_NAV_BLOCKER_BACK_OFFSET_TOP 64.0f
#endif

// Shared base-only geometry helper. The base keeps Entity.position as its
// render/world pivot, while interaction geometry targets a point shifted
// back into the visible base body.
static inline Vector2 base_interaction_anchor(const Entity *base) {
    if (!base) return (Vector2){ 0.0f, 0.0f };

    Vector2 anchor = base->position;
    if (base->presentationSide == SIDE_TOP) {
        anchor.y -= BASE_INTERACTION_BACK_OFFSET;
    } else if (base->presentationSide == SIDE_BOTTOM) {
        anchor.y += BASE_INTERACTION_BACK_OFFSET;
    }
    return anchor;
}

static inline Vector2 base_nav_blocker_center(const Entity *base) {
    if (!base) return (Vector2){ 0.0f, 0.0f };

    Vector2 center = base->position;
    if (base->presentationSide == SIDE_TOP) {
        center.y -= BASE_NAV_BLOCKER_BACK_OFFSET_TOP;
    } else if (base->presentationSide == SIDE_BOTTOM) {
        center.y += BASE_NAV_BLOCKER_BACK_OFFSET_BOTTOM;
    }
    return center;
}

// Tight authored hard core for the king/base sprite. This is intentionally
// smaller than BASE_NAV_RADIUS so the hard blocker grid matches the visible
// body, while outer approach and deposit behavior continue to use the wider
// traffic radius.
#define BASE_NAV_HARD_CORE_RADIUS 52.0f
#define BASE_NAV_HARD_CORE_CELL_COUNT 12

static inline float base_nav_hard_core_radius(const Entity *base) {
    (void)base;
    return BASE_NAV_HARD_CORE_RADIUS;
}

static inline Vector2 base_nav_forward_dir(const Entity *base) {
    if (!base) return (Vector2){ 0.0f, 0.0f };
    return (base->presentationSide == SIDE_TOP)
        ? (Vector2){ 0.0f,  1.0f }
        : (Vector2){ 0.0f, -1.0f };
}

// Authored cell mask for the base hard core. The shape is a tapered phase 4-row
// body: narrow at the front lip and rear edge, wider across the middle rows.
// World-space points intentionally target the centers of the cells to stamp.
static inline bool base_nav_hard_core_cell_point(const Entity *base, int index,
                                                 Vector2 *outPoint) {
    if (!base || !outPoint) return false;

    Vector2 center = base_nav_blocker_center(base);
    Vector2 forward = base_nav_forward_dir(base);
    Vector2 rear = (Vector2){ -forward.x, -forward.y };

    switch (index) {
        case 0:
            *outPoint = (Vector2){ center.x - 16.0f, center.y + forward.y * 48.0f };
            return true;
        case 1:
            *outPoint = (Vector2){ center.x + 16.0f, center.y + forward.y * 48.0f };
            return true;
        case 2:
            *outPoint = (Vector2){ center.x - 48.0f, center.y + forward.y * 16.0f };
            return true;
        case 3:
            *outPoint = (Vector2){ center.x - 16.0f, center.y + forward.y * 16.0f };
            return true;
        case 4:
            *outPoint = (Vector2){ center.x + 16.0f, center.y + forward.y * 16.0f };
            return true;
        case 5:
            *outPoint = (Vector2){ center.x + 48.0f, center.y + forward.y * 16.0f };
            return true;
        case 6:
            *outPoint = (Vector2){ center.x - 48.0f, center.y + rear.y * 16.0f };
            return true;
        case 7:
            *outPoint = (Vector2){ center.x - 16.0f, center.y + rear.y * 16.0f };
            return true;
        case 8:
            *outPoint = (Vector2){ center.x + 16.0f, center.y + rear.y * 16.0f };
            return true;
        case 9:
            *outPoint = (Vector2){ center.x + 48.0f, center.y + rear.y * 16.0f };
            return true;
        case 10:
            *outPoint = (Vector2){ center.x - 16.0f, center.y + rear.y * 48.0f };
            return true;
        case 11:
            *outPoint = (Vector2){ center.x + 16.0f, center.y + rear.y * 48.0f };
            return true;
        default:
            return false;
    }
}

#endif // NFC_CARDGAME_BASE_GEOMETRY_H
