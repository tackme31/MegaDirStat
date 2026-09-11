#include "core/TreemapLayout.h"

#include <QTest>

#include <algorithm>
#include <cmath>

namespace
{

double area(const QRectF& r)
{
    return r.width() * r.height();
}

bool overlaps(const QRectF& a, const QRectF& b)
{
    constexpr double eps = 1e-6;
    return a.left() < b.right() - eps && b.left() < a.right() - eps && a.top() < b.bottom() - eps &&
           b.top() < a.bottom() - eps;
}

bool inside(const QRectF& inner, const QRectF& outer)
{
    constexpr double eps = 1e-6;
    return inner.left() >= outer.left() - eps && inner.top() >= outer.top() - eps &&
           inner.right() <= outer.right() + eps && inner.bottom() <= outer.bottom() + eps;
}

} // namespace

class TestTreemapLayout : public QObject
{
    Q_OBJECT

private slots:
    void squarifyTilesBoundsProportionally_data()
    {
        QTest::addColumn<std::vector<qint64>>("sizes");
        QTest::addColumn<QRectF>("bounds");
        QTest::newRow("single") << std::vector<qint64>{42} << QRectF(0, 0, 300, 200);
        QTest::newRow("paper example") << std::vector<qint64>{6, 6, 4, 3, 2, 2, 1}
                                       << QRectF(0, 0, 6, 4);
        QTest::newRow("tall") << std::vector<qint64>{50, 20, 20, 5, 3, 1, 1}
                              << QRectF(10, 20, 80, 600);
        QTest::newRow("skewed") << std::vector<qint64>{1000000, 1, 1, 1} << QRectF(0, 0, 100, 100);
    }

    void squarifyTilesBoundsProportionally()
    {
        QFETCH(std::vector<qint64>, sizes);
        QFETCH(QRectF, bounds);

        const std::vector<QRectF> rects = squarify(sizes, bounds);
        QCOMPARE(rects.size(), sizes.size());

        double total = 0;
        for (qint64 s : sizes)
        {
            total += static_cast<double>(s);
        }
        double covered = 0;
        for (std::size_t i = 0; i < rects.size(); ++i)
        {
            QVERIFY2(inside(rects[i], bounds), qPrintable(QString::number(i)));
            const double expected = area(bounds) * static_cast<double>(sizes[i]) / total;
            QVERIFY2(std::abs(area(rects[i]) - expected) < 1e-6 * area(bounds),
                     qPrintable(QStringLiteral("rect %1: %2 vs %3")
                                    .arg(i)
                                    .arg(area(rects[i]))
                                    .arg(expected)));
            covered += area(rects[i]);
            for (std::size_t j = i + 1; j < rects.size(); ++j)
            {
                QVERIFY2(!overlaps(rects[i], rects[j]), qPrintable(QStringLiteral("%1/%2").arg(i).arg(j)));
            }
        }
        QVERIFY(std::abs(covered - area(bounds)) < 1e-6 * area(bounds));
    }

    void squarifyKeepsAspectRatiosReasonable()
    {
        // The worked example from Bruls et al.: no rectangle worse than 3:1.
        const std::vector<QRectF> rects = squarify({6, 6, 4, 3, 2, 2, 1}, QRectF(0, 0, 6, 4));
        for (const QRectF& r : rects)
        {
            const double ratio = std::max(r.width() / r.height(), r.height() / r.width());
            QVERIFY2(ratio <= 3.0 + 1e-9, qPrintable(QString::number(ratio)));
        }
    }

    void squarifyHandlesDegenerateInput()
    {
        QVERIFY(squarify({}, QRectF(0, 0, 10, 10)).empty());
        const auto rects = squarify({1, 2}, QRectF(0, 0, 0, 10));
        QCOMPARE(rects.size(), std::size_t(2));
        QVERIFY(rects[0].isNull());
    }

    void layoutLeavesTileTheBounds()
    {
        SizeNode root;
        root.kind = NodeKind::Folder;
        SizeNode* drive = root.addFolder(QStringLiteral("Cloud Drive"));
        SizeNode* photos = drive->addFolder(QStringLiteral("Photos"));
        for (int i = 1; i <= 20; ++i)
        {
            photos->addFile(QStringLiteral("p%1.jpg").arg(i), i * 1000);
        }
        drive->addFile(QStringLiteral("video.mp4"), 90000);
        drive->addFile(QStringLiteral("zero.txt"), 0);
        drive->addFolder(QStringLiteral("empty"));
        root.addFolder(QStringLiteral("Rubbish Bin"))->addFile(QStringLiteral("x.bin"), 5000);
        finalizeTree(root);

        const QRectF bounds(0, 0, 640, 480);
        const std::vector<TreemapCell> cells = layoutTreemap(root, bounds);
        QVERIFY(!cells.empty());
        QCOMPARE(cells.front().node, &root);
        QCOMPARE(cells.front().depth, 0);

        double leafArea = 0;
        int leaves = 0;
        for (const TreemapCell& cell : cells)
        {
            QVERIFY(cell.node->size > 0);
            QVERIFY(inside(cell.rect, bounds));
            if (cell.leaf)
            {
                ++leaves;
                leafArea += area(cell.rect);
                QVERIFY(!cell.node->isFolder());
            }
            if (cell.node->parent && cell.node != &root)
            {
                // Pre-order: the parent's cell comes first and contains the child.
                const auto parentIt =
                    std::find_if(cells.begin(), cells.end(), [&](const TreemapCell& c) {
                        return c.node == cell.node->parent;
                    });
                QVERIFY(parentIt != cells.end());
                QVERIFY(parentIt < cells.end() && &*parentIt < &cell);
                QVERIFY(inside(cell.rect, parentIt->rect));
                QCOMPARE(cell.depth, parentIt->depth + 1);
            }
        }
        QCOMPARE(leaves, 22);
        QVERIFY(std::abs(leafArea - area(bounds)) < 1e-6 * area(bounds));
    }

    void tinyFoldersBecomeLeaves()
    {
        SizeNode root;
        root.kind = NodeKind::Folder;
        root.addFile(QStringLiteral("huge.bin"), 1'000'000'000);
        SizeNode* folder = root.addFolder(QStringLiteral("tiny"));
        folder->addFile(QStringLiteral("a"), 1);
        folder->addFile(QStringLiteral("b"), 1);
        finalizeTree(root);

        const std::vector<TreemapCell> cells = layoutTreemap(root, QRectF(0, 0, 100, 100));
        const auto it = std::find_if(cells.begin(), cells.end(), [&](const TreemapCell& c) {
            return c.node == folder;
        });
        QVERIFY(it != cells.end());
        QVERIFY(it->leaf);
        QVERIFY(std::none_of(cells.begin(), cells.end(), [&](const TreemapCell& c) {
            return c.node->parent == folder;
        }));
    }

    void paddingInsetsFolderContent()
    {
        SizeNode root;
        root.kind = NodeKind::Folder;
        SizeNode* folder = root.addFolder(QStringLiteral("f"));
        SizeNode* file = folder->addFile(QStringLiteral("a"), 1);
        finalizeTree(root);

        TreemapOptions options;
        options.padding = 3;
        const std::vector<TreemapCell> cells = layoutTreemap(root, QRectF(0, 0, 100, 50), options);
        QCOMPARE(cells.size(), std::size_t(3));
        // The invisible root is not framed, its first level is.
        QCOMPARE(cells[1].node, folder);
        QCOMPARE(cells[1].rect, QRectF(0, 0, 100, 50));
        QCOMPARE(cells[2].node, file);
        QCOMPARE(cells[2].rect, QRectF(3, 3, 94, 44));
    }

    void paddingSkippedWhenFolderIsTooSmall()
    {
        SizeNode root;
        root.kind = NodeKind::Folder;
        SizeNode* folder = root.addFolder(QStringLiteral("f"));
        folder->addFile(QStringLiteral("a"), 1);
        finalizeTree(root);

        TreemapOptions options;
        options.padding = 3;
        const std::vector<TreemapCell> cells = layoutTreemap(root, QRectF(0, 0, 10, 10), options);
        QCOMPARE(cells.size(), std::size_t(3));
        QCOMPARE(cells[2].rect, QRectF(0, 0, 10, 10));
    }
};

QTEST_GUILESS_MAIN(TestTreemapLayout)
#include "tst_treemaplayout.moc"
