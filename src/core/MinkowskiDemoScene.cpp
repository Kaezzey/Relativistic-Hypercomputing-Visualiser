#include "core/MinkowskiDemoScene.h"

namespace rhv::core
{
models::MinkowskiDiagramScene BuildMinkowskiDemoScene()
{
    models::MinkowskiDiagramScene scene{};
    scene.modelName = "MINKOWSKI DEMO / FLAT SPACETIME";
    scene.unitConvention = "UNITS C = 1 / X AND T SHARE SCALE";
    scene.simplificationNote =
        "Toy model: 1+1 dimensional flat spacetime only. No acceleration, curvature, or 3D optical effects are included.";
    scene.defaultSelectedEventIndex = 0;
    scene.events = {
        models::SpacetimeEvent{
            "O0",
            0.0,
            0.0,
            models::Tone::Active,
            "Reference origin event. Use this as the first light-cone anchor."},
        models::SpacetimeEvent{
            "A1",
            -1.5,
            2.8,
            models::Tone::Structural,
            "Timelike future of O0. A slower-than-light signal from O0 could reach it."},
        models::SpacetimeEvent{
            "N1",
            2.0,
            2.0,
            models::Tone::Warning,
            "Null-separated from O0. This sits exactly on a light-speed path in the toy model."},
        models::SpacetimeEvent{
            "S1",
            3.2,
            1.6,
            models::Tone::Muted,
            "Spacelike relative to O0. It is outside O0's light cone."},
        models::SpacetimeEvent{
            "P1",
            -0.8,
            -2.1,
            models::Tone::Structural,
            "Timelike past of O0. It could influence O0."},
        models::SpacetimeEvent{
            "N0",
            -1.4,
            -1.4,
            models::Tone::Warning,
            "Null-separated in the past. This lies on a past light-speed path for O0."},
    };

    return scene;
}
}  // namespace rhv::core
