#include "LoggingChannel.hpp"

#include "common/QLogging.hpp"
#include "messages/Message.hpp"
#include "messages/MessageThread.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"

#include <QDir>

namespace chatterino {

QByteArray endline("\n");

LoggingChannel::LoggingChannel(const QString &_channelName,
                               const QString &_platform)
    : channelName(_channelName)
    , platform(_platform)
{
    if (this->channelName.startsWith("/whispers"))
    {
        this->subDirectory = "Whispers";
    }
    else if (channelName.startsWith("/mentions"))
    {
        this->subDirectory = "Mentions";
    }
    else if (channelName.startsWith("/live"))
    {
        this->subDirectory = "Live";
    }
    else
    {
        this->subDirectory =
            QStringLiteral("Channels") + QDir::separator() + channelName;
    }

    // enforce capitalized platform names
    this->subDirectory = platform[0].toUpper() + platform.mid(1).toLower() +
                         QDir::separator() + this->subDirectory;

    getSettings()->logPath.connect([this](const QString &logPath, auto) {
        this->baseDirectory =
            logPath.isEmpty() ? getPaths()->messageLogDirectory : logPath;
        this->openLogFile();
    });
}

LoggingChannel::~LoggingChannel()
{
    this->appendLine(this->generateClosingString());
    this->fileHandle.close();
}

void LoggingChannel::openLogFile()
{
    QDateTime now = QDateTime::currentDateTime();
    this->dateString = this->generateDateString(now);

    if (this->fileHandle.isOpen())
    {
        this->fileHandle.flush();
        this->fileHandle.close();
    }

    QString baseFileName = this->channelName + "-" + this->dateString + ".log";

    QString directory =
        this->baseDirectory + QDir::separator() + this->subDirectory;

    if (!QDir().mkpath(directory))
    {
        qCDebug(chatterinoHelper) << "Unable to create logging path";
        return;
    }

    // Open file handle to log file of current date
    QString fileName = directory + QDir::separator() + baseFileName;
    qCDebug(chatterinoHelper) << "Logging to" << fileName;
    this->fileHandle.setFileName(fileName);

    this->fileHandle.open(QIODevice::Append);

    this->appendLine(this->generateOpeningString(now));
}

void LoggingChannel::addMessage(MessagePtr message)
{
    QDateTime now = QDateTime::currentDateTime();

    QString messageDateString = this->generateDateString(now);
    if (messageDateString != this->dateString)
    {
        this->dateString = messageDateString;
        this->openLogFile();
    }

    QString str;
    if (channelName.startsWith("/mentions"))
    {
        str.append("#" + message->channelName + " ");
    }

    str.append('[');
    str.append(now.toString("HH:mm:ss"));
    str.append("] ");

    QString messageText;
    if (message->loginName.isEmpty())
    {
        // This accounts for any messages not explicitly sent by a user, like
        // system messages, parts of announcements, subs etc.
        messageText = message->messageText;
    }
    else
    {
        if (message->localizedName.isEmpty())
        {
            messageText = message->loginName + ": " + message->messageText;
        }
        else
        {
            messageText = message->localizedName + " " + message->loginName +
                          ": " + message->messageText;
        }
    }

    if ((message->flags.has(MessageFlag::ReplyMessage) &&
         getSettings()->stripReplyMention) &&
        !getSettings()->hideReplyContext)
    {
        qsizetype colonIndex = messageText.indexOf(':');
        if (colonIndex != -1)
        {
            QString rootMessageChatter;
            if (message->replyParent)
            {
                rootMessageChatter = message->replyParent->loginName;
            }
            else
            {
                // we actually want to use 'reply-parent-user-login' tag here,
                // but it's not worth storing just for this edge case
                rootMessageChatter = message->replyThread->root()->loginName;
            }
            messageText.insert(colonIndex + 1, " @" + rootMessageChatter);
        }
    }
    str.append(messageText);
    str.append(endline);

    this->appendLine(str);
}

QString LoggingChannel::generateOpeningString(const QDateTime &now) const
{
    QString ret("# Start logging at ");

    ret.append(now.toString("yyyy-MM-dd HH:mm:ss "));
    ret.append(now.timeZoneAbbreviation());
    ret.append(endline);

    return ret;
}

QString LoggingChannel::generateClosingString(const QDateTime &now) const
{
    QString ret("# Stop logging at ");

    ret.append(now.toString("yyyy-MM-dd HH:mm:ss"));
    ret.append(now.timeZoneAbbreviation());
    ret.append(endline);

    return ret;
}

void LoggingChannel::appendLine(const QString &line)
{
    this->fileHandle.write(line.toUtf8());
    this->fileHandle.flush();
}

QString LoggingChannel::generateDateString(const QDateTime &now)
{
    return now.toString("yyyy-MM-dd");
}

}  // namespace chatterino
