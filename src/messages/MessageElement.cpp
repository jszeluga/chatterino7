#include "messages/MessageElement.hpp"

#include "Application.hpp"
#include "controllers/moderationactions/ModerationAction.hpp"
#include "debug/Benchmark.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "messages/layouts/MessageLayoutContainer.hpp"
#include "messages/layouts/MessageLayoutElement.hpp"
#include "providers/emoji/Emojis.hpp"
#include "singletons/Emotes.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "util/DebugCount.hpp"
#include "util/Variant.hpp"

#include <memory>

namespace chatterino {

namespace {

    // Computes the bounding box for the given vector of images
    QSize getBoundingBoxSize(const std::vector<ImagePtr> &images)
    {
        int width = 0;
        int height = 0;
        for (const auto &img : images)
        {
            width = std::max(width, img->width());
            height = std::max(height, img->height());
        }

        return QSize(width, height);
    }

}  // namespace

MessageElement::MessageElement(MessageElementFlags flags)
    : flags_(flags)
{
    DebugCount::increase("message elements");
}

MessageElement::~MessageElement()
{
    DebugCount::decrease("message elements");
}

MessageElement *MessageElement::setLink(const Link &link)
{
    this->link_ = link;
    return this;
}

MessageElement *MessageElement::setText(const QString &text)
{
    this->text_ = text;
    return this;
}

MessageElement *MessageElement::setTooltip(const QString &tooltip)
{
    this->tooltip_ = tooltip;
    return this;
}

MessageElement *MessageElement::setThumbnail(const ImagePtr &thumbnail)
{
    this->thumbnail_ = thumbnail;
    return this;
}

MessageElement *MessageElement::setThumbnailType(const ThumbnailType type)
{
    this->thumbnailType_ = type;
    return this;
}

MessageElement *MessageElement::setTrailingSpace(bool value)
{
    this->trailingSpace = value;
    return this;
}

const QString &MessageElement::getTooltip() const
{
    return this->tooltip_;
}

const ImagePtr &MessageElement::getThumbnail() const
{
    return this->thumbnail_;
}

const MessageElement::ThumbnailType &MessageElement::getThumbnailType() const
{
    return this->thumbnailType_;
}

const QString &MessageElement::getText() const
{
    return this->text_;
}

const Link &MessageElement::getLink() const
{
    return this->link_;
}

bool MessageElement::hasTrailingSpace() const
{
    return this->trailingSpace;
}

MessageElementFlags MessageElement::getFlags() const
{
    return this->flags_;
}

void MessageElement::addFlags(MessageElementFlags flags)
{
    this->flags_.set(flags);
}

MessageElement *MessageElement::updateLink()
{
    this->linkChanged.invoke();
    return this;
}

void MessageElement::cloneFrom(const MessageElement &source)
{
    this->text_ = source.text_;
    this->link_ = source.link_;
    this->tooltip_ = source.tooltip_;
    this->thumbnail_ = source.thumbnail_;
    this->thumbnailType_ = source.thumbnailType_;
    this->flags_ = source.flags_;
}

// Empty
EmptyElement::EmptyElement()
    : MessageElement(MessageElementFlag::None)
{
}

void EmptyElement::addToContainer(MessageLayoutContainer &container,
                                  MessageElementFlags flags)
{
}

std::unique_ptr<MessageElement> EmptyElement::clone() const
{
    auto el = std::make_unique<EmptyElement>();
    el->cloneFrom(*this);
    return el;
}

EmptyElement &EmptyElement::instance()
{
    static EmptyElement instance;
    return instance;
}

// IMAGE
ImageElement::ImageElement(ImagePtr image, MessageElementFlags flags)
    : MessageElement(flags)
    , image_(image)
{
    //    this->setTooltip(image->getTooltip());
}

void ImageElement::addToContainer(MessageLayoutContainer &container,
                                  MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        auto size = QSize(this->image_->width() * container.getScale(),
                          this->image_->height() * container.getScale());

        container.addElement((new ImageLayoutElement(*this, this->image_, size))
                                 ->setLink(this->getLink()));
    }
}

std::unique_ptr<MessageElement> ImageElement::clone() const
{
    auto el = std::make_unique<ImageElement>(this->image_, this->getFlags());
    el->cloneFrom(*this);
    return el;
}

CircularImageElement::CircularImageElement(ImagePtr image, int padding,
                                           QColor background,
                                           MessageElementFlags flags)
    : MessageElement(flags)
    , image_(image)
    , padding_(padding)
    , background_(background)
{
}

void CircularImageElement::addToContainer(MessageLayoutContainer &container,
                                          MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        auto imgSize = QSize(this->image_->width(), this->image_->height()) *
                       container.getScale();

        container.addElement((new ImageWithCircleBackgroundLayoutElement(
                                  *this, this->image_, imgSize,
                                  this->background_, this->padding_))
                                 ->setLink(this->getLink()));
    }
}

std::unique_ptr<MessageElement> CircularImageElement::clone() const
{
    auto el = std::make_unique<CircularImageElement>(
        this->image_, this->padding_, this->background_, this->getFlags());
    el->cloneFrom(*this);
    return el;
}

// EMOTE
EmoteElement::EmoteElement(const EmotePtr &emote, MessageElementFlags flags,
                           const MessageColor &textElementColor)
    : MessageElement(flags)
    , emote_(emote)
{
    this->textElement_.reset(new TextElement(
        emote->getCopyString(), MessageElementFlag::Misc, textElementColor));

    this->setTooltip(emote->tooltip.string);
}

EmotePtr EmoteElement::getEmote() const
{
    return this->emote_;
}

void EmoteElement::addToContainer(MessageLayoutContainer &container,
                                  MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        if (flags.has(MessageElementFlag::EmoteImages))
        {
            auto image =
                this->emote_->images.getImageOrLoaded(container.getScale());
            if (image->isEmpty())
                return;

            auto emoteScale = getSettings()->emoteScale.getValue();

            auto size =
                QSize(int(container.getScale() * image->width() * emoteScale),
                      int(container.getScale() * image->height() * emoteScale));

            container.addElement(this->makeImageLayoutElement(image, size)
                                     ->setLink(this->getLink()));
        }
        else
        {
            if (this->textElement_)
            {
                this->textElement_->addToContainer(container,
                                                   MessageElementFlag::Misc);
            }
        }
    }
}

MessageLayoutElement *EmoteElement::makeImageLayoutElement(
    const ImagePtr &image, const QSize &size)
{
    return new ImageLayoutElement(*this, image, size);
}

std::unique_ptr<MessageElement> EmoteElement::clone() const
{
    auto el = std::make_unique<EmoteElement>(this->emote_, this->getFlags());
    el->textElement_ = std::unique_ptr<TextElement>(
        dynamic_cast<TextElement *>(this->textElement_->clone().release()));
    el->cloneFrom(*this);
    return el;
}

LayeredEmoteElement::LayeredEmoteElement(
    std::vector<LayeredEmoteElement::Emote> &&emotes, MessageElementFlags flags,
    const MessageColor &textElementColor)
    : MessageElement(flags)
    , emotes_(std::move(emotes))
    , textElementColor_(textElementColor)
{
    this->updateTooltips();
}

void LayeredEmoteElement::addEmoteLayer(const LayeredEmoteElement::Emote &emote)
{
    this->emotes_.push_back(emote);
    this->updateTooltips();
}

void LayeredEmoteElement::addToContainer(MessageLayoutContainer &container,
                                         MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        if (flags.has(MessageElementFlag::EmoteImages))
        {
            auto images = this->getLoadedImages(container.getScale());
            if (images.empty())
            {
                return;
            }

            auto emoteScale = getSettings()->emoteScale.getValue();
            float overallScale = emoteScale * container.getScale();

            auto largestSize = getBoundingBoxSize(images) * overallScale;
            std::vector<QSize> individualSizes;
            individualSizes.reserve(this->emotes_.size());
            for (auto img : images)
            {
                individualSizes.push_back(QSize(img->width(), img->height()) *
                                          overallScale);
            }

            container.addElement(this->makeImageLayoutElement(
                                         images, individualSizes, largestSize)
                                     ->setLink(this->getLink()));
        }
        else
        {
            if (this->textElement_)
            {
                this->textElement_->addToContainer(container,
                                                   MessageElementFlag::Misc);
            }
        }
    }
}

std::vector<ImagePtr> LayeredEmoteElement::getLoadedImages(float scale)
{
    std::vector<ImagePtr> res;
    res.reserve(this->emotes_.size());

    for (const auto &emote : this->emotes_)
    {
        auto image = emote.ptr->images.getImageOrLoaded(scale);
        if (image->isEmpty())
        {
            continue;
        }
        res.push_back(image);
    }
    return res;
}

MessageLayoutElement *LayeredEmoteElement::makeImageLayoutElement(
    const std::vector<ImagePtr> &images, const std::vector<QSize> &sizes,
    QSize largestSize)
{
    return new LayeredImageLayoutElement(*this, images, sizes, largestSize);
}

void LayeredEmoteElement::updateTooltips()
{
    if (!this->emotes_.empty())
    {
        QString copyStr = this->getCopyString();
        this->textElement_.reset(new TextElement(
            copyStr, MessageElementFlag::Misc, this->textElementColor_));
        this->setTooltip(copyStr);
    }

    std::vector<QString> result;
    result.reserve(this->emotes_.size());

    for (const auto &emote : this->emotes_)
    {
        result.push_back(emote.ptr->tooltip.string);
    }

    this->emoteTooltips_ = std::move(result);
}

const std::vector<QString> &LayeredEmoteElement::getEmoteTooltips() const
{
    return this->emoteTooltips_;
}

QString LayeredEmoteElement::getCleanCopyString() const
{
    QString result;
    for (size_t i = 0; i < this->emotes_.size(); ++i)
    {
        if (i != 0)
        {
            result += " ";
        }
        result += TwitchEmotes::cleanUpEmoteCode(
            this->emotes_[i].ptr->getCopyString());
    }
    return result;
}

QString LayeredEmoteElement::getCopyString() const
{
    QString result;
    for (size_t i = 0; i < this->emotes_.size(); ++i)
    {
        if (i != 0)
        {
            result += " ";
        }
        result += this->emotes_[i].ptr->getCopyString();
    }
    return result;
}

const std::vector<LayeredEmoteElement::Emote> &LayeredEmoteElement::getEmotes()
    const
{
    return this->emotes_;
}

std::vector<LayeredEmoteElement::Emote> LayeredEmoteElement::getUniqueEmotes()
    const
{
    // Functor for std::copy_if that keeps track of seen elements
    struct NotDuplicate {
        bool operator()(const Emote &element)
        {
            return seen.insert(element.ptr).second;
        }

    private:
        std::set<EmotePtr> seen;
    };

    // Get unique emotes while maintaining relative layering order
    NotDuplicate dup;
    std::vector<Emote> unique;
    std::copy_if(this->emotes_.begin(), this->emotes_.end(),
                 std::back_insert_iterator(unique), dup);

    return unique;
}

const MessageColor &LayeredEmoteElement::textElementColor() const
{
    return this->textElementColor_;
}

std::unique_ptr<MessageElement> LayeredEmoteElement::clone() const
{
    auto emotes = this->getEmotes();
    auto el = std::make_unique<LayeredEmoteElement>(
        std::move(emotes), this->getFlags(), this->textElementColor());
    el->cloneFrom(*this);
    return el;
}

// BADGE
BadgeElement::BadgeElement(const EmotePtr &emote, MessageElementFlags flags)
    : MessageElement(flags)
    , emote_(emote)
{
    this->setTooltip(emote->tooltip.string);
}

void BadgeElement::addToContainer(MessageLayoutContainer &container,
                                  MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        auto image =
            this->emote_->images.getImageOrLoaded(container.getScale());
        if (image->isEmpty())
            return;

        auto size = QSize(int(container.getScale() * image->width()),
                          int(container.getScale() * image->height()));

        container.addElement(this->makeImageLayoutElement(image, size));
    }
}

EmotePtr BadgeElement::getEmote() const
{
    return this->emote_;
}

MessageLayoutElement *BadgeElement::makeImageLayoutElement(
    const ImagePtr &image, const QSize &size)
{
    auto element =
        (new ImageLayoutElement(*this, image, size))->setLink(this->getLink());

    return element;
}

std::unique_ptr<MessageElement> BadgeElement::clone() const
{
    auto el = std::make_unique<BadgeElement>(this->emote_, this->getFlags());
    el->cloneFrom(*this);
    return el;
}

// MOD BADGE
ModBadgeElement::ModBadgeElement(const EmotePtr &data,
                                 MessageElementFlags flags_)
    : BadgeElement(data, flags_)
{
}

MessageLayoutElement *ModBadgeElement::makeImageLayoutElement(
    const ImagePtr &image, const QSize &size)
{
    static const QColor modBadgeBackgroundColor("#34AE0A");

    auto element = (new ImageWithBackgroundLayoutElement(
                        *this, image, size, modBadgeBackgroundColor))
                       ->setLink(this->getLink());

    return element;
}

std::unique_ptr<MessageElement> ModBadgeElement::clone() const
{
    auto el = std::make_unique<ModBadgeElement>(this->emote_, this->getFlags());
    el->cloneFrom(*this);
    return el;
}

// VIP BADGE
VipBadgeElement::VipBadgeElement(const EmotePtr &data,
                                 MessageElementFlags flags_)
    : BadgeElement(data, flags_)
{
}

MessageLayoutElement *VipBadgeElement::makeImageLayoutElement(
    const ImagePtr &image, const QSize &size)
{
    auto element =
        (new ImageLayoutElement(*this, image, size))->setLink(this->getLink());

    return element;
}

std::unique_ptr<MessageElement> VipBadgeElement::clone() const
{
    auto el = std::make_unique<VipBadgeElement>(this->emote_, this->getFlags());
    el->cloneFrom(*this);
    return el;
}

// FFZ Badge
FfzBadgeElement::FfzBadgeElement(const EmotePtr &data,
                                 MessageElementFlags flags_, QColor color_)
    : BadgeElement(data, flags_)
    , color(std::move(color_))
{
}

MessageLayoutElement *FfzBadgeElement::makeImageLayoutElement(
    const ImagePtr &image, const QSize &size)
{
    auto element =
        (new ImageWithBackgroundLayoutElement(*this, image, size, this->color))
            ->setLink(this->getLink());

    return element;
}

std::unique_ptr<MessageElement> FfzBadgeElement::clone() const
{
    auto el = std::make_unique<FfzBadgeElement>(this->emote_, this->getFlags(),
                                                this->color);
    el->cloneFrom(*this);
    return el;
}

// TEXT
TextElement::TextElement(const QString &text, MessageElementFlags flags,
                         const MessageColor &color, FontStyle style)
    : MessageElement(flags)
    , color_(color)
    , style_(style)
{
    for (const auto &word : text.split(' '))
    {
        this->words_.push_back({word, -1});
        // fourtf: add logic to store multiple spaces after message
    }
}

TextElement::TextElement(std::vector<Word> &&words, MessageElementFlags flags,
                         const MessageColor &color, FontStyle style)
    : MessageElement(flags)
    , color_(color)
    , style_(style)
    , words_(std::move(words))
{
}

MessageColor TextElement::color() const
{
    return this->color_;
}

FontStyle TextElement::style() const
{
    return this->style_;
}

const std::vector<TextElement::Word> &TextElement::words() const
{
    return this->words_;
}

void TextElement::addToContainer(MessageLayoutContainer &container,
                                 MessageElementFlags flags)
{
    auto app = getApp();

    if (flags.hasAny(this->getFlags()))
    {
        QFontMetrics metrics =
            app->fonts->getFontMetrics(this->style_, container.getScale());

        for (Word &word : this->words_)
        {
            auto getTextLayoutElement = [&](QString text, int width,
                                            bool hasTrailingSpace) {
                auto color = this->color_.getColor(*app->themes);
                app->themes->normalizeColor(color);

                auto e = (new TextLayoutElement(
                              *this, text, QSize(width, metrics.height()),
                              color, this->style_, container.getScale()))
                             ->setLink(this->getLink());
                e->setTrailingSpace(hasTrailingSpace);
                e->setText(text);

                // If URL link was changed,
                // Should update it in MessageLayoutElement too!
                if (this->getLink().type == Link::Url)
                {
                    static_cast<TextLayoutElement *>(e)->listenToLinkChanges();
                }
                return e;
            };

            // fourtf: add again
            //            if (word.width == -1) {
            word.width = metrics.horizontalAdvance(word.text);
            //            }

            // see if the text fits in the current line
            if (container.fitsInLine(word.width))
            {
                container.addElementNoLineBreak(getTextLayoutElement(
                    word.text, word.width, this->hasTrailingSpace()));
                continue;
            }

            // see if the text fits in the next line
            if (!container.atStartOfLine())
            {
                container.breakLine();

                if (container.fitsInLine(word.width))
                {
                    container.addElementNoLineBreak(getTextLayoutElement(
                        word.text, word.width, this->hasTrailingSpace()));
                    continue;
                }
            }

            // we done goofed, we need to wrap the text
            QString text = word.text;
            int textLength = text.length();
            int wordStart = 0;
            int width = 0;

            // QChar::isHighSurrogate(text[0].unicode()) ? 2 : 1

            for (int i = 0; i < textLength; i++)
            {
                auto isSurrogate = text.size() > i + 1 &&
                                   QChar::isHighSurrogate(text[i].unicode());

                auto charWidth = isSurrogate
                                     ? metrics.horizontalAdvance(text.mid(i, 2))
                                     : metrics.horizontalAdvance(text[i]);

                if (!container.fitsInLine(width + charWidth))
                {
                    container.addElementNoLineBreak(getTextLayoutElement(
                        text.mid(wordStart, i - wordStart), width, false));
                    container.breakLine();

                    wordStart = i;
                    width = charWidth;

                    if (isSurrogate)
                        i++;
                    continue;
                }

                width += charWidth;

                if (isSurrogate)
                    i++;
            }
            //add the final piece of wrapped text
            container.addElementNoLineBreak(getTextLayoutElement(
                text.mid(wordStart), width, this->hasTrailingSpace()));
        }
    }
}

std::unique_ptr<MessageElement> TextElement::clone() const
{
    auto el = std::make_unique<TextElement>(QString(), this->getFlags(),
                                            this->color_, this->style_);
    el->words_ = this->words_;
    el->cloneFrom(*this);
    return el;
}

SingleLineTextElement::SingleLineTextElement(const QString &text,
                                             MessageElementFlags flags,
                                             const MessageColor &color,
                                             FontStyle style)
    : MessageElement(flags)
    , color_(color)
    , style_(style)
{
    for (const auto &word : text.split(' '))
    {
        this->words_.push_back({word, -1});
    }
}

void SingleLineTextElement::addToContainer(MessageLayoutContainer &container,
                                           MessageElementFlags flags)
{
    auto app = getApp();

    if (flags.hasAny(this->getFlags()))
    {
        QFontMetrics metrics =
            app->fonts->getFontMetrics(this->style_, container.getScale());

        auto getTextLayoutElement = [&](QString text, int width,
                                        bool hasTrailingSpace) {
            auto color = this->color_.getColor(*app->themes);
            app->themes->normalizeColor(color);

            auto e = (new TextLayoutElement(
                          *this, text, QSize(width, metrics.height()), color,
                          this->style_, container.getScale()))
                         ->setLink(this->getLink());
            e->setTrailingSpace(hasTrailingSpace);
            e->setText(text);

            // If URL link was changed,
            // Should update it in MessageLayoutElement too!
            if (this->getLink().type == Link::Url)
            {
                static_cast<TextLayoutElement *>(e)->listenToLinkChanges();
            }
            return e;
        };

        static const auto ellipsis = QStringLiteral("...");

        // String to continuously append words onto until we place it in the container
        // once we encounter an emote or reach the end of the message text. */
        QString currentText;

        container.first = FirstWord::Neutral;
        for (Word &word : this->words_)
        {
            for (const auto &parsedWord : app->emotes->emojis.parse(word.text))
            {
                if (parsedWord.type() == typeid(QString))
                {
                    if (!currentText.isEmpty())
                    {
                        currentText += ' ';
                    }
                    currentText += boost::get<QString>(parsedWord);
                    QString prev =
                        currentText;  // only increments the ref-count
                    currentText =
                        metrics.elidedText(currentText, Qt::ElideRight,
                                           container.remainingWidth());
                    if (currentText != prev)
                    {
                        break;
                    }
                }
                else if (parsedWord.type() == typeid(EmotePtr))
                {
                    auto emote = boost::get<EmotePtr>(parsedWord);
                    auto image =
                        emote->images.getImageOrLoaded(container.getScale());
                    if (!image->isEmpty())
                    {
                        auto emoteScale = getSettings()->emoteScale.getValue();

                        int currentWidth =
                            metrics.horizontalAdvance(currentText);
                        auto emoteSize =
                            QSize(image->width(), image->height()) *
                            (emoteScale * container.getScale());

                        if (!container.fitsInLine(currentWidth +
                                                  emoteSize.width()))
                        {
                            currentText += ellipsis;
                            break;
                        }

                        // Add currently pending text to container, then add the emote after.
                        container.addElementNoLineBreak(getTextLayoutElement(
                            currentText, currentWidth, false));
                        currentText.clear();

                        container.addElementNoLineBreak(
                            (new ImageLayoutElement(*this, image, emoteSize))
                                ->setLink(this->getLink()));
                    }
                }
            }
        }

        // Add the last of the pending message text to the container.
        if (!currentText.isEmpty())
        {
            // Remove trailing space.
            currentText = currentText.trimmed();

            int width = metrics.horizontalAdvance(currentText);
            container.addElementNoLineBreak(
                getTextLayoutElement(currentText, width, false));
        }

        container.breakLine();
    }
}

std::unique_ptr<MessageElement> SingleLineTextElement::clone() const
{
    auto el = std::make_unique<SingleLineTextElement>(
        QString(), this->getFlags(), this->color_, this->style_);
    el->words_ = this->words_;
    el->cloneFrom(*this);
    return el;
}

// TIMESTAMP
TimestampElement::TimestampElement(QTime time)
    : MessageElement(MessageElementFlag::Timestamp)
    , time_(time)
    , element_(this->formatTime(time))
{
    assert(this->element_ != nullptr);
}

void TimestampElement::addToContainer(MessageLayoutContainer &container,
                                      MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        if (getSettings()->timestampFormat != this->format_)
        {
            this->format_ = getSettings()->timestampFormat.getValue();
            this->element_.reset(this->formatTime(this->time_));
        }

        this->element_->addToContainer(container, flags);
    }
}

TextElement *TimestampElement::formatTime(const QTime &time)
{
    static QLocale locale("en_US");

    QString format = locale.toString(time, getSettings()->timestampFormat);

    return new TextElement(format, MessageElementFlag::Timestamp,
                           MessageColor::System, FontStyle::ChatMedium);
}

std::unique_ptr<MessageElement> TimestampElement::clone() const
{
    auto el = std::make_unique<TimestampElement>(this->time_);
    el->cloneFrom(*this);
    return el;
}

// TWITCH MODERATION
TwitchModerationElement::TwitchModerationElement()
    : MessageElement(MessageElementFlag::ModeratorTools)
{
}

void TwitchModerationElement::addToContainer(MessageLayoutContainer &container,
                                             MessageElementFlags flags)
{
    if (flags.has(MessageElementFlag::ModeratorTools))
    {
        QSize size(int(container.getScale() * 16),
                   int(container.getScale() * 16));
        auto actions = getSettings()->moderationActions.readOnly();
        for (const auto &action : *actions)
        {
            if (auto image = action.getImage())
            {
                container.addElement(
                    (new ImageLayoutElement(*this, *image, size))
                        ->setLink(Link(Link::UserAction, action.getAction())));
            }
            else
            {
                container.addElement(
                    (new TextIconLayoutElement(*this, action.getLine1(),
                                               action.getLine2(),
                                               container.getScale(), size))
                        ->setLink(Link(Link::UserAction, action.getAction())));
            }
        }
    }
}

std::unique_ptr<MessageElement> TwitchModerationElement::clone() const
{
    auto el = std::make_unique<TwitchModerationElement>();
    el->cloneFrom(*this);
    return el;
}

LinebreakElement::LinebreakElement(MessageElementFlags flags)
    : MessageElement(flags)
{
}

void LinebreakElement::addToContainer(MessageLayoutContainer &container,
                                      MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        container.breakLine();
    }
}

std::unique_ptr<MessageElement> LinebreakElement::clone() const
{
    auto el = std::make_unique<LinebreakElement>(this->getFlags());
    el->cloneFrom(*this);
    return el;
}

ScalingImageElement::ScalingImageElement(ImageSet images,
                                         MessageElementFlags flags)
    : MessageElement(flags)
    , images_(images)
{
}

void ScalingImageElement::addToContainer(MessageLayoutContainer &container,
                                         MessageElementFlags flags)
{
    if (flags.hasAny(this->getFlags()))
    {
        const auto &image =
            this->images_.getImageOrLoaded(container.getScale());
        if (image->isEmpty())
            return;

        auto size = QSize(image->width() * container.getScale(),
                          image->height() * container.getScale());

        container.addElement((new ImageLayoutElement(*this, image, size))
                                 ->setLink(this->getLink()));
    }
}

std::unique_ptr<MessageElement> ScalingImageElement::clone() const
{
    auto el =
        std::make_unique<ScalingImageElement>(this->images_, this->getFlags());
    el->cloneFrom(*this);
    return el;
}

ReplyCurveElement::ReplyCurveElement()
    : MessageElement(MessageElementFlag::RepliedMessage)
{
}

void ReplyCurveElement::addToContainer(MessageLayoutContainer &container,
                                       MessageElementFlags flags)
{
    static const int width = 18;         // Overall width
    static const float thickness = 1.5;  // Pen width
    static const int radius = 6;         // Radius of the top left corner
    static const int margin = 2;         // Top/Left/Bottom margin

    if (flags.hasAny(this->getFlags()))
    {
        float scale = container.getScale();
        container.addElement(
            new ReplyCurveLayoutElement(*this, width * scale, thickness * scale,
                                        radius * scale, margin * scale));
    }
}

std::unique_ptr<MessageElement> ReplyCurveElement::clone() const
{
    auto el = std::make_unique<ReplyCurveElement>();
    el->cloneFrom(*this);
    return el;
}

}  // namespace chatterino
