/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "instrumentsreader.h"

#include <QXmlStreamReader>
#include <QApplication>

#include "libmscore/utils.h"
#include "libmscore/xml.h"

using namespace mu;
using namespace mu::instruments;
using namespace mu::midi;

RetVal<InstrumentsMeta> InstrumentsReader::readMeta(const io::path& path) const
{
    RetVal<InstrumentsMeta> result;

    RetVal<QByteArray> fileBytes = fileSystem()->readFile(path);

    if (!fileBytes.ret) {
        result.ret = fileBytes.ret;
        return result;
    }

    InstrumentsMeta meta;
    Ms::XmlReader reader(fileBytes.val);

    int groupIndex = 0;

    while (reader.readNextStartElement()) {
        if (reader.name() != "museScore") {
            continue;
        }

        while (reader.readNextStartElement()) {
            if (reader.name() == "instrument-group" || reader.name() == "InstrumentGroup") {
                loadGroupMeta(reader, meta, groupIndex++);
            } else if (reader.name() == "Articulation") {
                MidiArticulation articulation = readArticulation(reader);
                meta.articulations.insert(articulation.name, articulation); // TODO: name?
            } else if (reader.name() == "Genre") {
                InstrumentGenre genre = readGenre(reader);
                meta.genres.insert(genre.id, genre);
            } else if (reader.name() == "Family") {
                InstrumentFamily family = readFamily(reader);
                meta.families.insert(family.id, family);
            } else if (reader.name() == "Order") {
                ScoreOrder order = readScoreOrder(reader);
                order.index = meta.scoreOrders.size();
                meta.scoreOrders.insert(order.id, order);
            } else {
                reader.skipCurrentElement();
            }
        }
    }

    result.ret = make_ret(Ret::Code::Ok);
    result.val = meta;

    return result;
}

void InstrumentsReader::loadGroupMeta(Ms::XmlReader& reader, InstrumentsMeta& generalMeta, int groupIndex) const
{
    InstrumentGroup group;
    group.id = reader.attributes().value("id").toString();
    group.name = qApp->translate("InstrumentsXML", reader.attributes().value("name").toUtf8().data()); // TODO: translate
    group.extended = reader.attributes().value("extended").toInt();
    group.sequenceOrder = groupIndex;

    while (reader.readNextStartElement()) {
        if (reader.name().toString().toLower() == "instrument") {
            InstrumentTemplate _template = readInstrumentTemplate(reader, generalMeta);
            _template.instrument.groupId = group.id;
            generalMeta.instrumentTemplates.insert(_template.id, _template);
        } else if (reader.name() == "ref") {
            QString templateId = reader.readElementText();
            InstrumentTemplate newTemplate = generalMeta.instrumentTemplates[templateId];
            generalMeta.instrumentTemplates.insert(newTemplate.id, newTemplate);
        } else if (reader.name() == "name") {
            group.name = qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data()); // TODO: translate
        } else if (reader.name() == "extended") {
            group.extended = reader.readElementText().toInt();
        } else {
            reader.skipCurrentElement();
        }
    }

    if (group.id.isEmpty()) {
        group.id = group.name.toLower().replace(" ", "-");
    }

    generalMeta.groups.insert(group.id, group);
}

MidiArticulation InstrumentsReader::readArticulation(Ms::XmlReader& reader) const
{
    MidiArticulation articulation;
    articulation.name = reader.attributes().value("name").toString();

    while (reader.readNextStartElement()) {
        if (reader.name() == "velocity") {
            QString text(reader.readElementText());
            if (text.endsWith("%")) {
                text = text.left(text.size() - 1);
            }
            articulation.velocity = text.toInt();
        } else if (reader.name() == "gateTime") {
            QString text(reader.readElementText());
            if (text.endsWith("%")) {
                text = text.left(text.size() - 1);
            }
            articulation.gateTime = text.toInt();
        } else if (reader.name() == "descr") {
            articulation.descr = reader.readElementText();
        } else {
            reader.skipCurrentElement();
        }
    }

    return articulation;
}

InstrumentGenre InstrumentsReader::readGenre(Ms::XmlReader& reader) const
{
    InstrumentGenre genre;
    genre.id = reader.attributes().value("id").toString();

    while (reader.readNextStartElement()) {
        if (reader.name() == "name") {
            genre.name = qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data());
        } else {
            reader.skipCurrentElement();
        }
    }

    return genre;
}

InstrumentFamily InstrumentsReader::readFamily(Ms::XmlReader& reader) const
{
    InstrumentFamily family;
    family.id = reader.attributes().value("id").toString();

    while (reader.readNextStartElement()) {
        if (reader.name() == "name") {
            family.name = qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data());
        } else {
            reader.skipCurrentElement();
        }
    }

    return family;
}

ScoreOrder InstrumentsReader::readScoreOrder(Ms::XmlReader& reader) const
{
    ScoreOrder order;

    order.id = reader.attributes().value("id").toString();

    while (reader.readNextStartElement()) {
        if (reader.name() == "name") {
            order.name = qApp->translate("OrderXML", reader.readElementText().toUtf8().data());
        } else if (reader.name() == "instrument") {
            QString instrumentId = reader.attributes().value("id").toString();
            InstrumentOverwrite overwrite;
            while (reader.readNextStartElement()) {
                if (reader.name() == "family") {
                    overwrite.id = reader.attributes().value("id").toString();
                    overwrite.name = reader.readElementText();
                } else {
                    reader.skipCurrentElement();
                }
            }
            order.instrumentMap.insert(instrumentId, overwrite);
        } else if (reader.name() == "family") {
            ScoreOrderGroup sg;
            sg.index = order.groups.size();
            sg.family = reader.readElementText();
            order.groups << sg;
        } else if (reader.name() == "soloists") {
            ScoreOrderGroup sg;
            sg.index = order.groups.size();
            sg.family = QString("<soloists>");
            order.groups << sg;
            reader.skipCurrentElement();
        } else if (reader.name() == "unsorted") {
            ScoreOrderGroup sg;
            sg.index = order.groups.size();
            sg.family = QString("<unsorted>");
            sg.unsorted = reader.attribute("group", QString(""));
            order.groups << sg;
            reader.skipCurrentElement();
        } else if (reader.name() == "section") {
            QString section = reader.attributes().value("id").toString();
            while (reader.readNextStartElement()) {
                if (reader.name() == "family") {
                    ScoreOrderGroup sg;
                    sg.index = order.groups.size();
                    sg.family = reader.readElementText();
                    sg.section = section;
                    sg.bracket = true;
                    sg.showSystemMarkings = readBoolAttribute(reader, "showSystemMarkings", false);
                    sg.barLineSpan = readBoolAttribute(reader, "barLineSpan", true);
                    sg.thinBracket = readBoolAttribute(reader, "thinBrackets", true);
                    order.groups << sg;
                } else if (reader.name() == "soloists") {
                    ScoreOrderGroup sg;
                    sg.index = order.groups.size();
                    sg.family = QString("<soloists>");
                    sg.section = section;
                    order.groups << sg;
                    reader.skipCurrentElement();
                } else if (reader.name() == "unsorted") {
                    ScoreOrderGroup sg;
                    sg.index = order.groups.size();
                    sg.family = QString("<unsorted>");
                    sg.unsorted = reader.attribute("group", QString(""));
                    order.groups << sg;
                    reader.skipCurrentElement();
                } else {
                    reader.skipCurrentElement();
                }
            }
        } else {
            reader.skipCurrentElement();
        }
    }

    return order;
}

bool InstrumentsReader::readBoolAttribute(Ms::XmlReader& reader, const char* name, bool defvalue) const
{
    if (!reader.hasAttribute(name)) {
        return defvalue;
    }
    QString attr { reader.attribute(name) };
    if (attr.toLower() == "false") {
        return false;
    } else if (attr.toLower() == "true") {
        return true;
    } else {
        return defvalue;
    }
}

InstrumentTemplate InstrumentsReader::readInstrumentTemplate(Ms::XmlReader& reader, InstrumentsMeta& generalMeta) const
{
    InstrumentTemplate instrumentTemplate;
    Instrument& instrument = instrumentTemplate.instrument;

    instrument.id = reader.attributes().value("id").toString();
    instrumentTemplate.id = instrument.id;

    instrument.sequenceOrder =  generalMeta.instrumentTemplates.size();

    bool customDrumset = false;

    while (reader.readNextStartElement()) {
        if (reader.name() == "longName" || reader.name() == "name") {
            int pos = reader.intAttribute("pos", 0);
            for (auto it = instrument.longNames.begin(); it != instrument.longNames.end(); ++it) {
                if (it->pos() == pos) {
                    instrument.longNames.erase(it);
                    break;
                }
            }
            instrument.longNames << StaffName(qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data()), pos);
        } else if (reader.name() == "shortName" || reader.name() == "short-name") {
            int pos = reader.intAttribute("pos", 0);
            for (auto it = instrument.shortNames.begin(); it != instrument.shortNames.end(); ++it) {
                if (it->pos() == pos) {
                    instrument.shortNames.erase(it);
                    break;
                }
            }
            instrument.shortNames << StaffName(qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data()), pos);
        } else if (reader.name() == "trackName") {
            instrument.name = qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data());
        } else if (reader.name() == "description") {
            instrument.description = qApp->translate("InstrumentsXML", reader.readElementText().toUtf8().data());
        } else if (reader.name() == "extended") {
            instrument.extended = reader.readElementText().toInt();
        } else if (reader.name() == "staves") {
            instrument.staves = reader.readElementText().toInt();
            instrument.bracketSpan[0] = instrument.staves;
        } else if (reader.name() == "clef") {
            int staffIndex = readStaffIndex(reader);

            QString clef = reader.readElementText();
            bool ok = false;
            int clefNumber = clef.toInt(&ok);
            ClefType ct = ok ? ClefType(clefNumber) : Clef::clefType(clef);
            instrument.clefs[staffIndex]._concertClef = ct;
            instrument.clefs[staffIndex]._transposingClef = ct;
        } else if (reader.name() == "concertClef") {
            int staffIndex = readStaffIndex(reader);

            QString clef = reader.readElementText();
            bool ok = false;
            int clefNumber = clef.toInt(&ok);
            ClefType ct = ok ? ClefType(clefNumber) : Clef::clefType(clef);
            instrument.clefs[staffIndex]._concertClef = ct;
        } else if (reader.name() == "transposingClef") {
            int staffIndex = readStaffIndex(reader);

            QString clef = reader.readElementText();
            bool ok = false;
            int clefNumber = clef.toInt(&ok);
            ClefType ct = ok ? ClefType(clefNumber) : Clef::clefType(clef);
            instrument.clefs[staffIndex]._transposingClef = ct;
        } else if (reader.name() == "stafflines") {
            int staffIndex = readStaffIndex(reader);
            instrument.staffLines[staffIndex] = reader.readElementText().toInt();
        } else if (reader.name() == "smallStaff") {
            int staffIndex = readStaffIndex(reader);
            instrument.smallStaff[staffIndex] = reader.readElementText().toInt();
        } else if (reader.name() == "bracket") {
            int staffIndex = readStaffIndex(reader);
            instrument.bracket[staffIndex] = BracketType(reader.readElementText().toInt());
        } else if (reader.name() == "bracketSpan") {
            int staffIndex = readStaffIndex(reader);
            instrument.bracketSpan[staffIndex] = reader.readElementText().toInt();
        } else if (reader.name() == "barlineSpan") {
            int staffIndex = readStaffIndex(reader);
            int span = reader.readElementText().toInt();
            for (int i = 0; i < span - 1; ++i) {
                instrument.barlineSpan[staffIndex + i] = true;
            }
        } else if (reader.name() == "aPitchRange") {
            instrument.amateurPitchRange = readPitchRange(reader);
        } else if (reader.name() == "pPitchRange") {
            instrument.professionalPitchRange = readPitchRange(reader);
        } else if (reader.name() == "transposition") {
            instrument.transpose.chromatic = reader.readElementText().toInt();
            instrument.transpose.diatonic = Ms::chromatic2diatonic(instrument.transpose.chromatic);
        } else if (reader.name() == "transposeChromatic") {
            instrument.transpose.chromatic = reader.readElementText().toInt();
        } else if (reader.name() == "transposeDiatonic") {
            instrument.transpose.diatonic = reader.readElementText().toInt();
        } else if (reader.name() == "instrumentId") {
            instrument.musicXMLid = reader.readElementText();
        } else if (reader.name() == "StringData") {
            instrument.stringData = readStringData(reader);
        } else if (reader.name() == "useDrumset") {
            instrument.useDrumset = reader.readElementText().toInt();
            if (instrument.useDrumset) {
                delete instrument.drumset;
                instrument.drumset = new Drumset(*Ms::smDrumset);
            }
        } else if (reader.name() == "Drum") {
            if (!instrument.drumset) {
                instrument.drumset = new Drumset(*Ms::smDrumset);
            }
            if (!customDrumset) {
                const_cast<Drumset*>(instrument.drumset)->clear();
                customDrumset = true;
            }
            const_cast<Drumset*>(instrument.drumset)->load(reader);
        } else if (reader.name() == "MidiAction") {
            MidiAction action = readMidiAction(reader);
            instrument.midiActions << action;
        } else if (reader.name() == "Channel" || reader.name() == "channel") {
            Channel channel;
            channel.read(reader, nullptr);
            instrument.channels << channel;
        } else if (reader.name() == "Articulation") {
            MidiArticulation articulation = readArticulation(reader);
            generalMeta.articulations.insert(articulation.name, articulation);
        } else if (reader.name() == "stafftype") {
            int staffIndex = readStaffIndex(reader);

            QString xmlPresetName = reader.attributes().value("staffTypePreset").toString();
            QString stfGroup = reader.readElementText();
            if (stfGroup == "percussion") {
                instrument.staffGroup = StaffGroup::PERCUSSION;
            } else if (stfGroup == "tablature") {
                instrument.staffGroup = StaffGroup::TAB;
            } else {
                instrument.staffGroup = StaffGroup::STANDARD;
            }
            instrument.staffTypePreset = 0;
            if (!xmlPresetName.isEmpty()) {
                instrument.staffTypePreset = StaffType::presetFromXmlName(xmlPresetName);
            }
            if (!instrument.staffTypePreset || instrument.staffTypePreset->group() != instrument.staffGroup) {
                instrument.staffTypePreset = StaffType::getDefaultPreset(instrument.staffGroup);
            }
            if (instrument.staffTypePreset) {
                instrument.staffLines[staffIndex] = instrument.staffTypePreset->lines();
            }
        } else if (reader.name() == "init") {
            QString templateId = reader.readElementText();
            initInstrument(instrument, generalMeta.instrumentTemplates[templateId].instrument);
        } else if (reader.name() == "musicXMLid") {
            instrument.musicXMLid = reader.readElementText();
        } else if (reader.name() == "genre") {
            instrument.genreIds << reader.readElementText();
        } else if (reader.name() == "family") {
            instrument.familyId = reader.readElementText();
        } else if (reader.name() == "singleNoteDynamics") {
            instrument.singleNoteDynamics = reader.readElementText().toInt();
        } else {
            reader.skipCurrentElement();
        }
    }

    fillByDefault(instrument);

    return instrumentTemplate;
}

int InstrumentsReader::readStaffIndex(Ms::XmlReader& reader) const
{
    int staffIndex = reader.attributes().value("staff").toInt() - 1;
    if (staffIndex >= MAX_STAVES) {
        staffIndex = MAX_STAVES - 1;
    } else if (staffIndex < 0) {
        staffIndex = 0;
    }

    return staffIndex;
}

PitchRange InstrumentsReader::readPitchRange(Ms::XmlReader& reader) const
{
    PitchRange range;
    QStringList ranges = reader.readElementText().split("-");
    if (ranges.size() != 2) {
        range.min = 0;
        range.max = 127;
        return range;
    }
    range.min = ranges[0].toInt();
    range.max = ranges[1].toInt();

    return range;
}

MidiAction InstrumentsReader::readMidiAction(Ms::XmlReader& reader) const
{
    MidiAction action;
    action.name = reader.attributes().value("name").toString();

    while (reader.readNextStartElement()) {
        if (reader.name() == "program") {
            Event event(0 /*TODO*/, EventType::ME_CONTROLLER, CntrType::CTRL_PROGRAM,
                        reader.attributes().value("value").toInt());
            action.events.push_back(event);
            reader.skipCurrentElement();
        } else if (reader.name() == "controller") {
            Event event(0 /*TODO*/, EventType::ME_CONTROLLER, reader.attributes().value("ctrl").toInt(),
                        reader.attributes().value("value").toInt());
            action.events.push_back(event);
            reader.skipCurrentElement();
        } else if (reader.name() == "descr") {
            action.description = reader.readElementText();
        } else {
            reader.skipCurrentElement();
        }
    }

    return action;
}

StringData InstrumentsReader::readStringData(Ms::XmlReader& reader) const
{
    int frets = 0;
    QList<Ms::instrString> strings;

    while (reader.readNextStartElement()) {
        if (reader.name() == "frets") {
            frets = reader.readElementText().toInt();
        } else if (reader.name() == "string") {
            Ms::instrString strg;
            strg.open  = reader.attributes().value("open").toInt();
            strg.pitch = reader.readElementText().toInt();
            strings << strg;
        } else {
            reader.skipCurrentElement();
        }
    }

    return StringData(frets, strings);
}

void InstrumentsReader::fillByDefault(Instrument& instrument) const
{
    if (instrument.channels.empty()) {
        Channel a;
        a.setChorus(0);
        a.setReverb(0);
        a.setName(Channel::DEFAULT_NAME);
        a.setProgram(0);
        a.setBank(0);
        a.setVolume(90);
        a.setPan(0);
        instrument.channels.append(a);
    }

    if (instrument.name.isEmpty() && !instrument.longNames.isEmpty()) {
        instrument.name = instrument.longNames[0].name();
    }
    if (instrument.description.isEmpty() && !instrument.longNames.isEmpty()) {
        instrument.description = instrument.longNames[0].name();
    }
    if (instrument.id.isEmpty()) {
        instrument.id = instrument.name.toLower().replace(" ", "-");
    }
}

void InstrumentsReader::initInstrument(Instrument& sourceInstrument, const Instrument& destinationInstrument) const
{
    sourceInstrument.id = destinationInstrument.id;
    sourceInstrument.musicXMLid = destinationInstrument.musicXMLid;
    sourceInstrument.longNames = destinationInstrument.longNames;
    sourceInstrument.shortNames = destinationInstrument.shortNames;
    sourceInstrument.staves = destinationInstrument.staves;
    sourceInstrument.extended = destinationInstrument.extended;

    for (int i = 0; i < MAX_STAVES; ++i) {
        sourceInstrument.clefs[i] = destinationInstrument.clefs[i];
        sourceInstrument.staffLines[i] = destinationInstrument.staffLines[i];
        sourceInstrument.smallStaff[i] = destinationInstrument.smallStaff[i];
        sourceInstrument.bracket[i] = destinationInstrument.bracket[i];
        sourceInstrument.bracketSpan[i] = destinationInstrument.bracketSpan[i];
        sourceInstrument.barlineSpan[i] = destinationInstrument.barlineSpan[i];
    }

    sourceInstrument.amateurPitchRange = destinationInstrument.amateurPitchRange;
    sourceInstrument.professionalPitchRange = destinationInstrument.professionalPitchRange;
    sourceInstrument.transpose = destinationInstrument.transpose;
    sourceInstrument.staffGroup = destinationInstrument.staffGroup;
    sourceInstrument.staffTypePreset = destinationInstrument.staffTypePreset;
    sourceInstrument.useDrumset = destinationInstrument.useDrumset;

    if (destinationInstrument.drumset) {
        sourceInstrument.drumset = new Drumset(*destinationInstrument.drumset);
    }

    sourceInstrument.stringData = destinationInstrument.stringData;
    sourceInstrument.midiActions = destinationInstrument.midiActions;
    sourceInstrument.channels = destinationInstrument.channels;
    sourceInstrument.singleNoteDynamics = destinationInstrument.singleNoteDynamics;
}
