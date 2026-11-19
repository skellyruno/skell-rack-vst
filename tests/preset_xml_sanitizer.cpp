#include "../Source/Presets.h"

#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const char* label)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
    else
    {
        std::cout << "PASS: " << label << '\n';
    }
}
}

int main()
{
    juce::XmlElement xml ("ModularFXPreset");
    xml.setAttribute ("name", "Corrupt Import Regression");
    auto* fx = xml.createNewChildElement ("FX");
    fx->setAttribute ("type", ID_VisualEQ);
    fx->setAttribute ("on", 1);
    fx->setAttribute ("p0", "nan");
    fx->setAttribute ("p1", "inf");
    fx->setAttribute ("p2", "-inf");
    fx->setAttribute ("p3", 1.5);
    fx->setAttribute ("p4", -0.25);
    fx->setAttribute ("p5", 0.375);
    fx->setAttribute ("p6", 0.0);
    fx->setAttribute ("p7", 1.0);

    PresetDef imported;
    check (presetFromXml (xml, imported), "Import corrupt normalized parameter preset");
    check (imported.fx.size() == 1, "Known effect retained during import");

    if (imported.fx.size() == 1)
    {
        const auto& values = imported.fx.front().norm;
        bool allFinite = true;
        for (float value : values)
            allFinite = allFinite && std::isfinite (value);
        check (allFinite, "NaN and infinity never enter normalized preset state");
        check (values[0] == 0.5f && values[1] == 0.5f && values[2] == 0.5f,
               "Non-finite values fall back to the existing neutral default");
        check (values[3] == 1.0f && values[4] == 0.0f,
               "Finite out-of-range values clamp to legal endpoints");
        check (values[5] == 0.375f && values[6] == 0.0f && values[7] == 1.0f,
               "Valid continuous values survive unchanged");

        auto saved = presetToXml (imported);
        PresetDef restored;
        check (saved != nullptr && presetFromXml (*saved, restored),
               "Sanitized preset survives save/reload");
        if (restored.fx.size() == 1)
        {
            bool restoredFinite = true;
            for (float value : restored.fx.front().norm)
                restoredFinite = restoredFinite && std::isfinite (value);
            check (restoredFinite, "Second load remains finite");
            check (restored.fx.front().norm == values,
                   "Sanitized normalized values remain stable after roundtrip");
        }
        else
        {
            check (false, "Roundtrip retains imported effect");
        }
    }

    return failures == 0 ? 0 : 1;
}
