/**
 * This file is part of the libndef project.
 *
 * Copyright (C) 2009, Emanuele Bertoldi (Card Tech srl).
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 * $Revision: 8 $
 * $Date: 2010-01-06 15:04:09 +0100 (Wed, 06 Jan 2010) $
 */

#include "ndefrecord.h"
#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QStringList>

NDEFRecord::NDEFRecord()
    :   m_chuncked(false)
{
}

NDEFRecord::NDEFRecord(const QByteArray& data, const NDEFRecordType& type, int offset, bool chuncked)
        :   m_type(type),
            m_chuncked(chuncked)
{
    this->setPayload(data.right(data.size() - offset));
}

NDEFRecord::NDEFRecord(const NDEFRecordType& type, const QByteArray& id, const QByteArray& payload, bool chuncked)
        :   m_type(type),
            m_id(id),
            m_chuncked(chuncked)
{
    this->setPayload(payload);
}

NDEFRecord::~NDEFRecord()
{
}

void NDEFRecord::setId(const QByteArray& id)
{
    m_id = id;

    this->checkConsistency();
}

QByteArray NDEFRecord::id() const
{
    return m_id;
}

quint8 NDEFRecord::flags() const
{
    quint8 flags = 0;

    // Check the "short record" flag.
    flags |= this->isShort() ? NDEFRecord::NDEF_SR : 0;

    // Check the "LENGTH_ID" flag.
    flags |= (m_id.count() > 0) ? NDEFRecord::NDEF_IL : 0;

    // Check the "chunk" flag.
    flags |= this->isChuncked() ? NDEFRecord::NDEF_CF : 0;

    return flags;
}

bool NDEFRecord::isShort() const
{
    return ((m_payload.count() < 256) && (m_id.isEmpty()));
}

void NDEFRecord::setChuncked(bool flag)
{
    m_chuncked = flag;
}

bool NDEFRecord::isChuncked() const
{
    return m_chuncked;
}

bool NDEFRecord::isEmpty() const
{
    return (m_type.id() == NDEFRecordType::NDEF_Empty);
}

bool NDEFRecord::isValid() const
{
    return (m_type.id() != NDEFRecordType::NDEF_Invalid);
}

void NDEFRecord::setType(const NDEFRecordType& type)
{
    m_type = type;

    this->checkConsistency();
}

NDEFRecordType NDEFRecord::type() const
{
    return m_type;
}

void NDEFRecord::setPayload(const QByteArray& payload)
{
    m_payload = payload;

    this->checkConsistency();
}

void NDEFRecord::appendPayload(const QByteArray& payload)
{
    m_payload.append(payload);

    this->checkConsistency();
}

QByteArray NDEFRecord::payload() const
{
    return m_payload;
}

int NDEFRecord::payloadLength() const
{
    return m_payload.count();
}

QByteArray NDEFRecord::toByteArray(int flags) const
{
    QByteArray byte_array;
    QBuffer buffer(&byte_array);
    buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);
    
    // 1) Flags (5 bits) + TNF (3 bits)
    quint8 final_flags = (flags | this->flags());
    quint8 byte = (final_flags & 0xF8) | m_type.id();
    out << byte;
    
    // 2) Type length, payload length, ID length, type, ID and payload.
    switch (m_type.id())
    {
        // NDEF_Empty:
        // -- Type length = 0 (8 bits)
        // -- Payload length = 0 (8 bits)
        // -- ID length = 0 (8 bits)
        // -- No type
        // -- No ID
        // -- No payload
        case NDEFRecordType::NDEF_Empty:
        {
            out << (quint8)0;
            out << (quint8)0;
            out << (quint8)0;
        }
        break;
        
        // NDEF_NfcForumRTD, NDEF_MIME, NDEF_URI, NDEF_ExternalRTD:
        // -- Type length = 8 bits
        // -- Payload length = 8 or 32 bits
        // -- ID length = 8 bits (if present).
        // -- Type = (type length) bytes
        // -- ID = (ID length) bytes  (if present).
        // -- Payload = (Payload length) bytes
        case NDEFRecordType::NDEF_NfcForumRTD:
        case NDEFRecordType::NDEF_MIME:
        case NDEFRecordType::NDEF_URI:
        case NDEFRecordType::NDEF_ExternalRTD:
        {
            QByteArray type_name = m_type.name();
            
            out << (quint8)type_name.count();
            if (this->isShort())
            {
                out << (quint8)m_payload.count();
                byte_array.append(type_name);
                byte_array.append(m_payload);
            }
            else
            {
                out << (quint32)m_payload.count();
                out << (quint8)m_id.count();
                byte_array.append(type_name);
                byte_array.append(m_id);
                byte_array.append(m_payload);
            }
        }
        break;
        
        // NDEF_Unknown, NDEF_Unchanged:
        // -- Type length = 0 (8 bits)
        // -- Payload length = 32 bits
        // -- ID length = N bits
        // -- No type
        // -- ID = (id length) bytes
        // -- Payload = (payload length) bytes
        case NDEFRecordType::NDEF_Unknown:
        case NDEFRecordType::NDEF_Unchanged:
        {
            out << (quint8)0;
            out << (quint32)m_payload.count();
            out << (quint32)m_id.count();
            byte_array.append(m_id);
            byte_array.append(m_payload);
        }
        break;

        // NDEF Invalid: empty buffer.
        case NDEFRecordType::NDEF_Invalid:
            return QByteArray();
    }

    return buffer.data();
}

void NDEFRecord::checkConsistency()
{
    int payload_size = m_payload.count();

    // Check the record type.
    if (payload_size > 0 && (m_type.id() == NDEFRecordType::NDEF_Empty))
        m_type = NDEFRecordType(NDEFRecordType::NDEF_Unknown);
}

NDEFRecord NDEFRecord::fromByteArray(const QByteArray& data, int offset)
{
    NDEFRecordType type = NDEFRecordType::fromByteArray(data, offset);

    // 1) Type.
    NDEFRecord record;
    record.setType(type);

    if (type.id() != NDEFRecordType::NDEF_Invalid)
    {
        QByteArray data_buffer = data.right(data.count() - offset);
        QBuffer buffer(&data_buffer);
        buffer.open(QIODevice::ReadOnly);
        QDataStream stream(&buffer);
        quint8 byte;

        // 2) Flags.
        stream >> byte;
        quint8 flags = (quint8)byte;
        bool cf = flags & NDEFRecord::NDEF_CF;
        bool sr = flags & NDEFRecord::NDEF_SR;
        bool il = flags & NDEFRecord::NDEF_IL;
        record.setChuncked(cf);

        // 3) Type length.
        stream >> byte;
        quint8 type_length = byte;

        // 3) Payload length.
        quint32 payload_length = 0;
        if (sr)
        {
            stream >> byte;
            payload_length = (quint32)byte;
        }
        else
        {
            stream >> payload_length;
        }

        // 4) Skip type bytes.
        stream.skipRawData(type_length);

        // 5) ID.
        if (il)
        {
            stream >> byte;
            quint8 id_length = byte;
            record.setId(buffer.read(id_length));
        }

        // 6) Payload.
        record.setPayload(buffer.read(payload_length));
    }

    return record;
}

NDEFRecord NDEFRecord::createMimeRecord(const QString& mime_type, const QByteArray& payload)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType(NDEFRecordType::NDEF_MIME, mime_type.toAscii()));

    // 2) Payload.
    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createTextRecord(const QString& text, const QString& locale, NDEFRecordTextCodec codec)
{
    NDEFRecord record;

    int locale_size = qMin(locale.count(), 5);

    // 1) Type.
    record.setType(NDEFRecordType::textRecordType());

    // 2) Payload.
    QByteArray payload;
    char status_byte = 0;
    status_byte |= locale_size;
    status_byte |= (codec << 8);
    payload.append(status_byte);
    payload.append(locale.left(locale_size));
    payload.append(text);
    record.setPayload(payload);

    return record;
}

QByteArray NDEFRecord::textLocale(const QByteArray& payload)
{
    int locale_length = payload.at(0) & 0x1f;
    return payload.mid(1, locale_length);
}

NDEFRecord NDEFRecord::createUriRecord(const QString& uri)
{
    NDEFRecord record;

    int uri_size = uri.count();

    // 1) Type.
    record.setType(NDEFRecordType::uriRecordType());

    // 2) Payload.
    QByteArray payload;

    QStringList uri_identifiers;
    uri_identifiers.append("http://www.");
    uri_identifiers.append("https://www.");
    uri_identifiers.append("http://");
    uri_identifiers.append("https://");
    uri_identifiers.append("tel:");
    uri_identifiers.append("mailto:");
    uri_identifiers.append("ftp://anonymous:anonymous@");
    uri_identifiers.append("ftp://ftp.");
    uri_identifiers.append("ftps://");
    uri_identifiers.append("sftp://");
    uri_identifiers.append("smb://");
    uri_identifiers.append("nfs://");
    uri_identifiers.append("ftp://");
    uri_identifiers.append("dav://");
    uri_identifiers.append("news:");
    uri_identifiers.append("telnet://");
    uri_identifiers.append("imap:");
    uri_identifiers.append("rtsp://");
    uri_identifiers.append("urn:");
    uri_identifiers.append("pop:");
    uri_identifiers.append("sip:");
    uri_identifiers.append("sips:");
    uri_identifiers.append("tftp:");
    uri_identifiers.append("btspp://");
    uri_identifiers.append("btl2cap://");
    uri_identifiers.append("btgoep://");
    uri_identifiers.append("tcpobex://");
    uri_identifiers.append("irdaobex://");
    uri_identifiers.append("file://");
    uri_identifiers.append("urn:epc:id:");
    uri_identifiers.append("urn:epc:tag:");
    uri_identifiers.append("urn:epc:pat:");
    uri_identifiers.append("urn:epc:raw:");
    uri_identifiers.append("urn:epc:");
    uri_identifiers.append("urn:nfc:");

    int num_identifiers = uri_identifiers.count();
    char uri_identifier = 0;
    int uri_identifier_size = 0;
    for (int i = 0; i < num_identifiers; i++)
    {
        QString identifier = uri_identifiers.at(i);
        int size = identifier.count();
        if ((size > uri_identifier_size) && uri.startsWith(identifier))
        {
            uri_identifier_size = size;
            uri_identifier = i+1;
            break;
        }
    }

    payload.append(uri_identifier);
    payload.append(uri.right(uri_size - uri_identifier_size));
    record.setPayload(payload);

    return record;
}

QByteArray NDEFRecord::uriProtocol(const QByteArray& payload)
{
    QStringList uri_identifiers;
    uri_identifiers.append("http://www.");
    uri_identifiers.append("https://www.");
    uri_identifiers.append("http://");
    uri_identifiers.append("https://");
    uri_identifiers.append("tel:");
    uri_identifiers.append("mailto:");
    uri_identifiers.append("ftp://anonymous:anonymous@");
    uri_identifiers.append("ftp://ftp.");
    uri_identifiers.append("ftps://");
    uri_identifiers.append("sftp://");
    uri_identifiers.append("smb://");
    uri_identifiers.append("nfs://");
    uri_identifiers.append("ftp://");
    uri_identifiers.append("dav://");
    uri_identifiers.append("news:");
    uri_identifiers.append("telnet://");
    uri_identifiers.append("imap:");
    uri_identifiers.append("rtsp://");
    uri_identifiers.append("urn:");
    uri_identifiers.append("pop:");
    uri_identifiers.append("sip:");
    uri_identifiers.append("sips:");
    uri_identifiers.append("tftp:");
    uri_identifiers.append("btspp://");
    uri_identifiers.append("btl2cap://");
    uri_identifiers.append("btgoep://");
    uri_identifiers.append("tcpobex://");
    uri_identifiers.append("irdaobex://");
    uri_identifiers.append("file://");
    uri_identifiers.append("urn:epc:id:");
    uri_identifiers.append("urn:epc:tag:");
    uri_identifiers.append("urn:epc:pat:");
    uri_identifiers.append("urn:epc:raw:");
    uri_identifiers.append("urn:epc:");
    uri_identifiers.append("urn:nfc:");

    int protocol_index = payload.at(0);
    if (protocol_index > 0 && protocol_index <= uri_identifiers.count())
        return uri_identifiers.at(protocol_index-1).toAscii();

    return QByteArray();
}

NDEFRecord NDEFRecord::createSmartPosterRecord(const QString& uri)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::smartPosterRecordType());

    // 2) Payload.
    QByteArray payload = NDEFRecord::createUriRecord(uri).toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME);
    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createSmartPosterRecord(const QString& uri, const QString& title, const QString& locale, NDEFRecordTextCodec codec)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::smartPosterRecordType());

    // 2) Payload.
    QByteArray payload;
    payload.append(NDEFRecord::createTextRecord(title, locale, codec).toByteArray(NDEFRecord::NDEF_MB));
    payload.append(NDEFRecord::createUriRecord(uri).toByteArray(NDEFRecord::NDEF_ME));
    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createSmartPosterRecord(const QString& uri, const QList<NDEFRecord>& records)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::smartPosterRecordType());

    // 2) Payload.
    QByteArray payload;
    int record_count = records.count();
    for (int i = 0; i < record_count; i++)
    {
        // Add only valid records.
        NDEFRecord sp_record = records.at(i);
        NDEFRecordType type = sp_record.type();
        if (type.id() == NDEFRecordType::NDEF_MIME
            || type == NDEFRecordType::spActionRecordType()
            || type == NDEFRecordType::spSizeRecordType()
            || type == NDEFRecordType::spTypeRecordType()
            || type == NDEFRecordType::textRecordType())
        {

            int flags = (i == 0) ? NDEFRecord::NDEF_MB : 0;
            payload.append(sp_record.toByteArray(flags));
        }
    }
    int flags = (record_count == 0) ? (NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME) : NDEFRecord::NDEF_ME;
    payload.append(NDEFRecord::createUriRecord(uri).toByteArray(flags));
    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createSpActionRecord(NDEFRecordAction action)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::spActionRecordType());

    // 2) Payload.
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);
    stream << (quint8)action;
    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createSpSizeRecord(quint32 size)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::spSizeRecordType());

    // 2) Payload.
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);
    stream << size;
    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createSpTypeRecord(const QString& type)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::spTypeRecordType());

    // 2) Payload.
    record.setPayload(type.toAscii());

    return record;
}

NDEFRecord NDEFRecord::createGenericControlRecord(quint8 config_byte, const NDEFRecord& target, NDEFRecordAction action, const NDEFRecord& data)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::genericControlRecordType());

    // 2) Payload.
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);

    // 2.1) Config bytes.
    stream << config_byte;

    // 2.2) Target.
    NDEFRecord target_record = NDEFRecord::createGcTargetRecord(target);
    if (!target_record.isValid())
        return NDEFRecord(NDEFRecordType(NDEFRecordType::NDEF_Invalid, ""));

    payload.append(target_record.toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    // 2.3) Action.
    payload.append(NDEFRecord::createGcActionRecord(action).toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    // 2.4) Data.
    if (!data.isEmpty())
        payload.append(NDEFRecord::createGcDataRecord(data).toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createGenericControlRecord(quint8 config_byte, const NDEFRecord& target, const NDEFRecord& action, const NDEFRecord& data)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::genericControlRecordType());

    // 2) Payload.
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);

    // 2.1) Config bytes.
    stream << config_byte;

    // 2.2) Target.
    NDEFRecord target_record = NDEFRecord::createGcTargetRecord(target);
    if (!target_record.isValid())
        return NDEFRecord(NDEFRecordType(NDEFRecordType::NDEF_Invalid, ""));

    payload.append(target_record.toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    // 2.3) Action.
    if (!action.isEmpty())
        payload.append(NDEFRecord::createGcActionRecord(action).toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    // 2.4) Data.
    if (!data.isEmpty())
        payload.append(NDEFRecord::createGcDataRecord(data).toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createGcTargetRecord(const NDEFRecord& target)
{
    if (target.type() != NDEFRecordType::textRecordType()
        && target.type() != NDEFRecordType::uriRecordType())
        return NDEFRecord(NDEFRecordType(NDEFRecordType::NDEF_Invalid, ""));

    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::gcTargetRecordType());

    // 2) Payload.
    record.setPayload(target.toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    return record;
}

NDEFRecord NDEFRecord::createGcActionRecord(const NDEFRecord& action)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::gcActionRecordType());

    // 2) Payload.
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);

    // 2.1) Action Flag byte.
    stream << (quint8)0x0;

    // 2.2) Action record.
    payload.append(action.toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createGcActionRecord(NDEFRecordAction action)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::gcActionRecordType());

    // 2) Payload.
    QByteArray payload;
    QBuffer buffer(&payload);
    buffer.open(QIODevice::WriteOnly);
    QDataStream stream(&buffer);

    // 2.1) Action Flag byte.
    stream << (quint8)0x1;

    // 2.2) Action record.
    stream << (quint8)action;

    record.setPayload(payload);

    return record;
}

NDEFRecord NDEFRecord::createGcDataRecord(const NDEFRecord& data)
{
    NDEFRecord record;

    // 1) Type.
    record.setType(NDEFRecordType::gcDataRecordType());

    // 2) Payload.
    record.setPayload(data.toByteArray(NDEFRecord::NDEF_MB | NDEFRecord::NDEF_ME));

    return record;
}
