// atvstpresets: one .vstpreset per factory preset of a built VST3, for hosts that ignore VST3 program lists
// (Ableton Live shows only "Default" for our plugins) but list preset files from the VST3 preset folders.
//
//   atvstpresets <plugin.vst3> --out <dir> [--factory <plugins/<p>/assets/presets>] [--clean] [--no-check]
//
// Writes <dir>/<plugin name>/<category>/<preset>.vstpreset, where <plugin name> is the VST3 class name ("Audio Tools
// AT20V") and <category> the "category" of the preset's JSON in --factory (no subfolder when it has none). Only programs
// whose name is one of those JSON files are written, so the user presets of the machine that builds never get in.
// Without --factory every program is written, without categories.
//
// File format (VST3 SDK, public.sdk/source/vst/vstpresetfile): 'VST3', int32 version 1, the processor class ID as 32
// ASCII hex chars, int64 offset of the chunk list; then the chunks: 'Comp' = exactly what the plugin's
// IComponent::getState writes after selecting the program (the JUCE wrapper: getStateInformation + its private-data
// trailer), 'Cont' = the controller state when the plugin writes one, 'Info' = XML metadata (MediaType, PlugInName,
// PlugInCategory, Name, MusicalCategory); and 'List' with id, offset and size (int64) of each chunk. Little endian.
// The Comp/Cont chunks and the class ID come from the SDK's own PresetFile::savePreset (through JUCE's getPreset), so
// they are what Cubase would save.
//
// Check (unless --no-check): a second instance, first switched to another program, loads each file the way a host does
// (the SDK's PresetFile::loadPreset: setState on the component, setComponentState and setState on the controller) and
// must end with the same plugin state (the XML, attribute order aside) and the same parameter values as selecting the
// program directly. Exit code 1 when a file fails.

#include <JuceHeader.h>
#include <cstdio>
#include <map>
#include <set>

namespace {

// Folder and file names that are valid on macOS, Windows and Linux.
juce::String legalName (juce::String s)
{
    for (auto c : juce::String ("\\/:*?\"<>|")) s = s.replaceCharacter (c, '-');
    s = s.removeCharacters ("\r\n\t");
    while (s.contains ("  ")) s = s.replace ("  ", " ");
    s = s.trim().trimCharactersAtEnd (". ");
    return s.isEmpty() ? juce::String ("Preset") : s;
}

struct Chunk { char id[4]; juce::MemoryBlock data; };

// The chunks of a .vstpreset (header 48 bytes, list at the offset of the header).
bool parsePreset (const juce::MemoryBlock& mb, juce::String& classId, std::vector<Chunk>& chunks)
{
    juce::MemoryInputStream in (mb, false);
    char magic[4] = {};
    if (in.read (magic, 4) != 4 || memcmp (magic, "VST3", 4) != 0) return false;
    if (in.readInt() != 1) return false;
    char cid[33] = {};
    if (in.read (cid, 32) != 32) return false;
    classId = juce::String (cid, 32);
    const auto listOffset = in.readInt64();
    if (listOffset <= 0 || listOffset >= (juce::int64) mb.getSize() || ! in.setPosition (listOffset)) return false;
    char lid[4] = {};
    if (in.read (lid, 4) != 4 || memcmp (lid, "List", 4) != 0) return false;
    const int n = in.readInt();
    for (int i = 0; i < n; ++i)
    {
        Chunk c;
        if (in.read (c.id, 4) != 4) return false;
        const auto off = in.readInt64(), size = in.readInt64();
        if (off < 0 || size < 0 || off + size > (juce::int64) mb.getSize()) return false;
        c.data.append (static_cast<const char*> (mb.getData()) + off, (size_t) size);
        chunks.push_back (std::move (c));
    }
    return true;
}

juce::MemoryBlock writePreset (const juce::String& classId, const std::vector<Chunk>& chunks)
{
    juce::MemoryOutputStream out;
    out.write ("VST3", 4);
    out.writeInt (1);
    out.write (classId.toRawUTF8(), 32);
    out.writeInt64 (0);                                  // the list offset, filled in below
    std::vector<std::pair<juce::int64, juce::int64>> pos;
    for (auto& c : chunks)
    {
        pos.push_back ({ (juce::int64) out.getPosition(), (juce::int64) c.data.getSize() });
        out.write (c.data.getData(), c.data.getSize());
    }
    const auto listOffset = (juce::int64) out.getPosition();
    out.write ("List", 4);
    out.writeInt ((int) chunks.size());
    for (size_t i = 0; i < chunks.size(); ++i)
    {
        out.write (chunks[i].id, 4);
        out.writeInt64 (pos[i].first);
        out.writeInt64 (pos[i].second);
    }
    auto mb = out.getMemoryBlock();
    auto le = juce::ByteOrder::swapIfBigEndian ((juce::uint64) listOffset);
    memcpy (static_cast<char*> (mb.getData()) + 40, &le, 8);
    return mb;
}

juce::String metaInfo (const juce::String& pluginName, const juce::String& pluginCategory, const juce::String& name, const juce::String& category)
{
    auto attr = [] (const char* id, const juce::String& v)
    {
        const auto esc = v.replace ("&", "&amp;").replace ("\"", "&quot;").replace ("<", "&lt;").replace (">", "&gt;");
        return "\t<Attribute id=\"" + juce::String (id) + "\" value=\"" + esc + "\" type=\"string\" flags=\"writeProtected\"/>\n";
    };
    juce::String x ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<MetaInfo>\n");
    x << attr ("MediaType", "VstPreset") << attr ("PlugInName", pluginName) << attr ("PlugInCategory", pluginCategory)
      << attr ("Name", name);
    if (category.isNotEmpty()) x << attr ("MusicalCategory", category);
    x << "</MetaInfo>\n";
    return x;
}

struct Instance
{
    std::unique_ptr<juce::AudioPluginInstance> p;
    juce::AudioPluginExtensions::VST3Client* vst3 = nullptr;
};

juce::AudioPluginFormatManager& formats()
{
    static juce::AudioPluginFormatManager fm;
    static bool init = false;
    if (! init) { fm.addFormat (std::make_unique<juce::VST3PluginFormat>()); init = true; }
    return fm;
}

Instance load (const juce::PluginDescription& d)
{
    Instance in;
    juce::String err;
    in.p = formats().createPluginInstance (d, 48000.0, 512, err);
    if (in.p == nullptr) { std::printf ("could not load: %s\n", err.toRawUTF8()); std::exit (2); }
    in.p->enableAllBuses();
    in.p->setRateAndBufferSizeDetails (48000.0, 512);
    in.p->prepareToPlay (48000.0, 512);
    in.vst3 = in.p->getVST3Client();
    if (in.vst3 == nullptr) { std::printf ("not a VST3\n"); std::exit (2); }
    return in;
}

// The plugin's own state (the XML inside the component state), with the attributes in a fixed order.
std::unique_ptr<juce::XmlElement> pluginXml (const juce::MemoryBlock& comp)
{
    return juce::AudioProcessor::getXmlFromBinary (comp.getData(), (int) comp.getSize());
}

juce::MemoryBlock componentState (juce::AudioPluginInstance& p)
{
    // The JUCE host keeps the component state as base64 in <IComponent> (what IComponent::getState wrote).
    juce::MemoryBlock mb, comp;
    p.getStateInformation (mb);
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        if (auto* c = xml->getChildByName ("IComponent")) comp.fromBase64Encoding (c->getAllSubText().trim());
    return comp;
}

// Two attribute values that are the same number up to float rounding (a value that went through the normalised
// 0..1 range of its parameter comes back a few ulps off: 0.49999997 vs 0.49999991).
bool sameNumber (const juce::String& a, const juce::String& b)
{
    auto isNum = [] (const juce::String& s) { return s.isNotEmpty() && s.containsOnly ("0123456789.-+eE") && s.containsAnyOf ("0123456789"); };
    if (! isNum (a) || ! isNum (b)) return false;
    const double x = a.getDoubleValue(), y = b.getDoubleValue();
    return std::abs (x - y) <= 1.0e-5 * std::max (1.0, std::max (std::abs (x), std::abs (y)));
}

// The differences between two XML trees, attribute order aside. Empty = equal.
void diffXml (const juce::XmlElement& a, const juce::XmlElement& b, const juce::String& path, juce::StringArray& out,
              const juce::StringArray& ignore = {})
{
    if (out.size() > 8) return;
    if (a.getTagName() != b.getTagName()) { out.add (path + ": tag " + a.getTagName() + " vs " + b.getTagName()); return; }
    std::map<juce::String, juce::String> aa, bb;
    for (int i = 0; i < a.getNumAttributes(); ++i) aa[a.getAttributeName (i)] = a.getAttributeValue (i);
    for (int i = 0; i < b.getNumAttributes(); ++i) bb[b.getAttributeName (i)] = b.getAttributeValue (i);
    const auto id = a.getStringAttribute ("id");
    const auto here = path + "/" + a.getTagName() + (id.isNotEmpty() ? "[" + id + "]" : juce::String());
    for (auto& k : ignore) { aa.erase (k); bb.erase (k); }
    for (auto& [k, v] : aa)
        if (bb.count (k) == 0) out.add (here + "@" + k + " missing after load");
        else if (bb[k] != v && ! sameNumber (v, bb[k])) out.add (here + "@" + k + ": " + v.substring (0, 60) + " vs " + bb[k].substring (0, 60));
    for (auto& [k, v] : bb) if (aa.count (k) == 0) out.add (here + "@" + k + " only after load");
    if (a.getNumChildElements() != b.getNumChildElements()) { out.add (here + ": " + juce::String (a.getNumChildElements()) + " vs " + juce::String (b.getNumChildElements()) + " children"); return; }
    for (int i = 0; i < a.getNumChildElements(); ++i) diffXml (*a.getChildElement (i), *b.getChildElement (i), here, out);
    if (a.getAllSubText() != b.getAllSubText() && a.getNumChildElements() == 0) out.add (here + ": text differs");
}

// The component state with some attributes of the plugin's XML root removed (instance data that a preset must not
// carry, e.g. the role Orrery resolved for the instance that wrote it). The JUCE private-data trailer is kept as is.
juce::MemoryBlock dropRootAttributes (const juce::MemoryBlock& comp, const juce::StringArray& drop)
{
    if (drop.isEmpty() || comp.getSize() < 8 || memcmp (comp.getData(), "VC2!", 4) != 0) return comp;
    const auto len = (size_t) juce::ByteOrder::littleEndianInt (static_cast<const char*> (comp.getData()) + 4);
    if (8 + len > comp.getSize()) return comp;
    auto xml = pluginXml (comp);
    if (xml == nullptr) return comp;
    for (auto& a : drop) xml->removeAttribute (a);
    juce::MemoryBlock out;
    juce::AudioProcessor::copyXmlToBinary (*xml, out);
    out.append (static_cast<const char*> (comp.getData()) + 8 + len, comp.getSize() - 8 - len);
    return out;
}

std::vector<float> paramValues (juce::AudioPluginInstance& p)
{
    std::vector<float> v;
    for (auto* prm : p.getParameters()) v.push_back (prm->getValue());
    return v;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    if (args.isEmpty() || ! args.contains ("--out"))
    {
        std::printf ("usage: atvstpresets <plugin.vst3> --out <dir> [--factory <presets json dir>] [--clean] [--no-check]\n");
        return 2;
    }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto path = cwd.getChildFile (args[0]).getFullPathName();
    juce::OwnedArray<juce::PluginDescription> types;
    formats().getFormat (0)->findAllTypesForFile (types, path);
    if (types.isEmpty()) { std::printf ("no VST3 in %s\n", path.toRawUTF8()); return 2; }
    const auto& desc = *types[0];

    // Factory presets: name -> category, from the JSON files the plugin embeds.
    std::map<juce::String, juce::String> factory;
    const bool haveFactory = args.contains ("--factory");
    // Root attributes of the plugin's state that the files leave out: --drop-attrs a,b or, next to the factory
    // presets, ../vstpreset.json {"dropAttributes": [...]} (plugin data, so the packaging scripts stay generic).
    auto drop = juce::StringArray::fromTokens (opt ("--drop-attrs", ""), ",", "");
    if (haveFactory)
    {
        const auto cfg = juce::JSON::parse (cwd.getChildFile (opt ("--factory", "")).getSiblingFile ("vstpreset.json"));
        if (auto* arr = cfg.getProperty ("dropAttributes", {}).getArray())
            for (auto& v : *arr) drop.addIfNotAlreadyThere (v.toString());
    }
    drop.removeEmptyStrings();
    if (haveFactory)
    {
        const auto dir = cwd.getChildFile (opt ("--factory", ""));
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            const auto v = juce::JSON::parse (f);
            if (! v.isObject()) continue;
            const auto name = v.getProperty ("name", "").toString();
            if (name.isNotEmpty() && factory.count (name) == 0) factory[name] = v.getProperty ("category", "").toString().trim();
        }
        if (factory.empty()) { std::printf ("%s: no factory presets in %s, nothing to write\n", desc.name.toRawUTF8(), dir.getFullPathName().toRawUTF8()); return 0; }
    }

    const auto root = cwd.getChildFile (opt ("--out", "")).getChildFile (legalName (desc.name));
    if (args.contains ("--clean")) root.deleteRecursively();
    root.createDirectory();

    auto a = load (desc);
    const int n = a.p->getNumPrograms();
    juce::String classId;
    {
        std::vector<Chunk> ch;
        if (! parsePreset (a.vst3->getPreset(), classId, ch)) { std::printf ("getPreset gave no valid preset\n"); return 2; }
    }

    struct Item { int program; juce::String name, category; juce::File file; std::unique_ptr<juce::XmlElement> ref; std::vector<float> params; };
    std::vector<Item> items;
    std::map<juce::String, int> seen;
    int skipped = 0;
    for (int k = 0; k < n; ++k)
    {
        const auto name = a.p->getProgramName (k);
        juce::String category;
        if (haveFactory)
        {
            auto it = factory.find (name);
            if (it == factory.end() || seen.count (name) > 0) { ++skipped; continue; }   // a user preset of this machine
            category = it->second;
        }
        else if (name.isEmpty() || seen.count (name) > 0) { ++skipped; continue; }
        seen[name] = k;

        a.p->setCurrentProgram (k);
        juce::String cid; std::vector<Chunk> ch;
        if (! parsePreset (a.vst3->getPreset(), cid, ch) || cid != classId) { std::printf ("FAIL %s: getPreset\n", name.toRawUTF8()); return 1; }
        std::vector<Chunk> keep;
        for (auto& c : ch)
        {
            if (memcmp (c.id, "Comp", 4) == 0) c.data = dropRootAttributes (c.data, drop);
            if (memcmp (c.id, "Comp", 4) == 0 || (memcmp (c.id, "Cont", 4) == 0 && c.data.getSize() > 0)) keep.push_back (c);
        }
        Chunk info; memcpy (info.id, "Info", 4);
        const auto xml = metaInfo (desc.name, desc.category, name, category);
        info.data.append (xml.toRawUTF8(), xml.getNumBytesAsUTF8());
        keep.push_back (std::move (info));

        auto dir = category.isNotEmpty() ? root.getChildFile (legalName (category)) : root;
        dir.createDirectory();
        auto file = dir.getChildFile (legalName (name) + ".vstpreset");
        for (int i = 2; file.existsAsFile(); ++i) file = dir.getChildFile (legalName (name) + " " + juce::String (i) + ".vstpreset");
        const auto bytes = writePreset (classId, keep);
        if (! file.replaceWithData (bytes.getData(), bytes.getSize()))
        { std::printf ("FAIL %s: cannot write %s\n", name.toRawUTF8(), file.getFullPathName().toRawUTF8()); return 1; }

        Item item { k, name, category, file, pluginXml (componentState (*a.p)), paramValues (*a.p) };
        items.push_back (std::move (item));
    }
    std::printf ("%s: %d presets written to %s (%d programs skipped%s)\n", desc.name.toRawUTF8(), (int) items.size(),
                 root.getFullPathName().toRawUTF8(), skipped, drop.isEmpty() ? "" : (", without " + drop.joinIntoString (",")).toRawUTF8());
    {
        std::set<std::vector<float>> distinct;
        for (auto& it : items) distinct.insert (it.params);
        std::printf ("  %d distinct parameter sets among them\n", (int) distinct.size());
    }
    if (args.contains ("--no-check") || items.empty()) return 0;

    // Load each file like a host does, in a second instance that is on another program first. The first one goes away
    // before: with Orrery, a second live instance would become a satellite and differ in its own settings.
    a.p.reset();
    auto b = load (desc);
    int fails = 0;
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto& it = items[i];
        b.p->setCurrentProgram (items[(i + 1) % items.size()].program);
        juce::MemoryBlock data;
        it.file.loadFileAsData (data);
        juce::String cid; std::vector<Chunk> ch;
        if (! parsePreset (data, cid, ch) || cid != classId) { std::printf ("FAIL %s: unreadable file\n", it.name.toRawUTF8()); ++fails; continue; }
        if (! b.vst3->setPreset (data)) { std::printf ("FAIL %s: setPreset (PresetFile::loadPreset) refused it\n", it.name.toRawUTF8()); ++fails; continue; }

        juce::StringArray diffs;
        auto got = pluginXml (componentState (*b.p));
        if (got == nullptr || it.ref == nullptr) diffs.add ("no plugin XML");
        else diffXml (*it.ref, *got, "", diffs, drop);
        // The JUCE host refreshes its parameter cache only through setStateInformation: a round trip of the state that
        // was just loaded (it does not change it) makes getValue() report what the controller has.
        { juce::MemoryBlock s; b.p->getStateInformation (s); b.p->setStateInformation (s.getData(), (int) s.getSize()); }
        const auto pv = paramValues (*b.p);
        int pdiff = 0; juce::String firstP;
        for (size_t k = 0; k < pv.size() && k < it.params.size(); ++k)
            if (std::abs (pv[k] - it.params[k]) > 1.0e-6f)
            {
                if (pdiff++ == 0) firstP = b.p->getParameters()[(int) k]->getName (40) + " " + juce::String (it.params[k], 4) + " vs " + juce::String (pv[k], 4);
            }
        if (pdiff > 0) diffs.add (juce::String (pdiff) + " parameters differ (" + firstP + ")");
        if (diffs.isEmpty()) continue;
        ++fails;
        std::printf ("FAIL %s: %s\n", it.name.toRawUTF8(), diffs.joinIntoString ("; ").toRawUTF8());
    }
    std::printf ("%s: %d/%d presets load to the same state as the program (%d FAIL)\n", desc.name.toRawUTF8(),
                 (int) items.size() - fails, (int) items.size(), fails);
    return fails > 0 ? 1 : 0;
}
