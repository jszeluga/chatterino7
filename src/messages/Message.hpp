#pragma once

#include "common/FlagsEnum.hpp"
#include "util/QStringHash.hpp"

#include <QColor>
#include <QTime>

#include <cinttypes>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace chatterino {
class MessageElement;
class MessageThread;
class Badge;
class ScrollbarHighlight;

enum class MessageFlag : int64_t {
    None = 0LL,
    System = (1LL << 0),
    Timeout = (1LL << 1),
    Highlighted = (1LL << 2),
    DoNotTriggerNotification = (1LL << 3),  // disable notification sound
    Centered = (1LL << 4),
    Disabled = (1LL << 5),
    DisableCompactEmotes = (1LL << 6),
    Collapsed = (1LL << 7),
    ConnectedMessage = (1LL << 8),
    DisconnectedMessage = (1LL << 9),
    Untimeout = (1LL << 10),
    PubSub = (1LL << 11),
    Subscription = (1LL << 12),
    DoNotLog = (1LL << 13),
    AutoMod = (1LL << 14),
    RecentMessage = (1LL << 15),
    Whisper = (1LL << 16),
    HighlightedWhisper = (1LL << 17),
    Debug = (1LL << 18),
    Similar = (1LL << 19),
    RedeemedHighlight = (1LL << 20),
    RedeemedChannelPointReward = (1LL << 21),
    ShowInMentions = (1LL << 22),
    FirstMessage = (1LL << 23),
    ReplyMessage = (1LL << 24),
    ElevatedMessage = (1LL << 25),
    SubscribedThread = (1LL << 26),
    CheerMessage = (1LL << 27),
    LiveUpdatesAdd = (1LL << 28),
    LiveUpdatesRemove = (1LL << 29),
    LiveUpdatesUpdate = (1LL << 30),
};
using MessageFlags = FlagsEnum<MessageFlag>;

struct Message;
using MessagePtr = std::shared_ptr<const Message>;
struct Message {
    Message();
    ~Message();

    Message(const Message &) = delete;
    Message &operator=(const Message &) = delete;

    Message(Message &&) = delete;
    Message &operator=(Message &&) = delete;

    // Making this a mutable means that we can update a messages flags,
    // while still keeping Message constant. This means that a message's flag
    // can be updated without the renderer being made aware, which might be bad.
    // This is a temporary effort until we can figure out what the right
    // const-correct way to deal with this is.
    // This might bring race conditions with it
    mutable MessageFlags flags;
    QTime parseTime;
    QString id;
    QString searchText;
    QString messageText;
    QString loginName;
    QString displayName;
    QString localizedName;
    QString timeoutUser;
    QString channelName;
    QColor usernameColor;
    QDateTime serverReceivedTime;
    std::vector<Badge> badges;
    std::unordered_map<QString, QString> badgeInfos;
    std::shared_ptr<QColor> highlightColor;
    // Each reply holds a reference to the thread. When every reply is dropped,
    // the reply thread will be cleaned up by the TwitchChannel.
    // The root of the thread does not have replyThread set.
    std::shared_ptr<MessageThread> replyThread;
    MessagePtr replyParent;
    uint32_t count = 1;
    std::vector<std::unique_ptr<MessageElement>> elements;
    std::vector<QString> seventvEventTargetEmotes;

    ScrollbarHighlight getScrollBarHighlight() const;

    /**
     * Clones this message. Before contructing the shared pointer, 
     * `fn` is called with a reference to the new message.
     *
     * @return An identical message, independent from this one.
     */
    std::shared_ptr<const Message> cloneWith(
        const std::function<void(Message &)> &fn) const;
};

}  // namespace chatterino
