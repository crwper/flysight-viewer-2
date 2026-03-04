#ifndef MOMENTMODEL_H
#define MOMENTMODEL_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

namespace FlySight {

class SessionModel;
struct DependencyKey;

// ─────────────────────────────── Trait enums
// These control how each dock renders and interacts with a moment.

enum class PositionSource { MouseInput, Attribute, External };
enum class Interaction { None, Drag, PlaybackAndDrag };
enum class PlotPresentation { VerticalLine, DashedLine, DashedLineOnDrag, None };
enum class PlotBubble { Bubble, None };
enum class MapPresentation { LargeDot, SmallDot, None };
enum class LegendVisibility { Visible, Hidden };

// ─────────────────────────────── MomentTraits
// Bundles all declarative properties for a moment.

struct MomentTraits {
    PositionSource positionSource = PositionSource::External;
    QString attributeKey;           // only meaningful when positionSource == Attribute
    Interaction interaction = Interaction::None;
    PlotPresentation plotPresentation = PlotPresentation::None;
    PlotBubble plotBubble = PlotBubble::None;
    MapPresentation mapPresentation = MapPresentation::None;
    LegendVisibility legendVisibility = LegendVisibility::Hidden;

    // Visual properties carried from MarkerDefinition
    QString shortLabel;
    QColor color;
};

// ─────────────────────────────── MomentRegistration
// Helper struct for batch registration via registerMoments().

struct MomentRegistration {
    QString id;
    QString label;
    MomentTraits traits;
    QString groupId;  // empty = static
};

// ─────────────────────────────── MomentModel

class MomentModel : public QObject
{
    Q_OBJECT

public:
    // Internal data for a single moment.
    struct Moment {
        QString id;
        QString label;
        MomentTraits traits;
        bool enabled = true;
        QString groupId;

        // Position state (for MouseInput / External moments).
        // For Attribute-sourced moments, positionUtc is unused; position
        // is read on demand from SessionData::getAttribute(traits.attributeKey).
        double positionUtc = 0.0;
        bool active = false;
        QSet<QString> targetSessions;
    };

    explicit MomentModel(QObject *parent = nullptr);

    // ── Registration ──
    void registerMoment(const QString &id, const QString &label,
                        const MomentTraits &traits,
                        const QString &groupId = QString());
    void registerMoments(const QVector<MomentRegistration> &moments);
    void unregisterMoment(const QString &id);
    void unregisterMomentGroup(const QString &groupId);

    // ── Queries ──
    bool hasMoment(const QString &id) const;
    Moment momentById(const QString &id) const;
    QVector<Moment> allMoments() const;
    QVector<Moment> enabledMoments() const;

    // ── Position updates (for MouseInput and External moments) ──
    void setMomentPosition(const QString &id, double utcSeconds,
                           const QSet<QString> &targetSessions, bool active);
    void setMomentActive(const QString &id, bool active);

    // ── Enablement ──
    void setMomentEnabled(const QString &id, bool enabled);
    bool isMomentEnabled(const QString &id) const;

    // ── Dependency wiring ──
    void setSessionModel(SessionModel *sessionModel);

signals:
    void momentsChanged();

private slots:
    void onDependencyChanged(const QString &sessionId, const DependencyKey &key);

private:
    int indexForId(const QString &id) const;
    void rebuildIndex();
    void rebuildWatchedKeys();

    QVector<Moment> m_moments;
    QHash<QString, int> m_indexById;

    // Watched attribute keys for Attribute-sourced moments.
    // Rebuilt whenever moments are registered or unregistered.
    QSet<QString> m_watchedAttributeKeys;

    SessionModel *m_sessionModel = nullptr;
};

} // namespace FlySight

#endif // MOMENTMODEL_H
