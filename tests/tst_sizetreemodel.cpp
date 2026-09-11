#include "ui/SizeTreeModel.h"

#include <QAbstractItemModelTester>
#include <QTest>

namespace
{

struct Tree
{
    SnapshotPtr snapshot;
    const SizeNode* drive = nullptr;
    const SizeNode* photos = nullptr;
    const SizeNode* trip = nullptr;
    const SizeNode* bin = nullptr;
};

Tree makeTree()
{
    auto snapshot = std::make_shared<AccountSnapshot>();
    snapshot->root = std::make_unique<SizeNode>();
    snapshot->root->kind = NodeKind::Folder;
    Tree t;
    SizeNode* drive = snapshot->root->addFolder(QStringLiteral("Cloud Drive"));
    SizeNode* photos = drive->addFolder(QStringLiteral("Photos"));
    SizeNode* trip = photos->addFolder(QStringLiteral("Trip"));
    trip->addFile(QStringLiteral("a.jpg"), 300);
    trip->addFile(QStringLiteral("b.jpg"), 200);
    photos->addFile(QStringLiteral("c.jpg"), 100);
    drive->addFile(QStringLiteral("notes.txt"), 50);
    SizeNode* bin = snapshot->root->addFolder(QStringLiteral("Rubbish Bin"));
    bin->addFile(QStringLiteral("old.zip"), 10);
    finalizeTree(*snapshot->root);
    t.drive = drive;
    t.photos = photos;
    t.trip = trip;
    t.bin = bin;
    t.snapshot = std::move(snapshot);
    return t;
}

} // namespace

class TestSizeTreeModel : public QObject
{
    Q_OBJECT

private slots:
    void consistentWithAndWithoutScope()
    {
        const Tree t = makeTree();
        SizeTreeModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setSnapshot(t.snapshot);
        model.setScope(t.photos);
        model.setScope(t.trip);
        model.setScope(nullptr);
        model.setScope(t.drive);
        model.setSnapshot(t.snapshot);
    }

    void unscopedShowsTopLevelTrees()
    {
        const Tree t = makeTree();
        SizeTreeModel model;
        model.setSnapshot(t.snapshot);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.nodeAt(model.index(0, 0)), t.drive);
        QCOMPARE(model.nodeAt(model.index(1, 0)), t.bin);
        QCOMPARE(model.indexFor(t.trip).data(SizeTreeModel::DepthRole).toInt(), 2);
    }

    void scopeIsTheOnlyRoot()
    {
        const Tree t = makeTree();
        SizeTreeModel model;
        model.setSnapshot(t.snapshot);
        model.setScope(t.photos);

        QCOMPARE(model.rowCount(), 1);
        const QModelIndex top = model.index(0, 0);
        QCOMPARE(model.nodeAt(top), t.photos);
        QVERIFY(!model.parent(top).isValid());
        QCOMPARE(model.indexFor(t.photos), top);
        QCOMPARE(top.data(SizeTreeModel::DepthRole).toInt(), 0);

        const QModelIndex trip = model.indexFor(t.trip);
        QCOMPARE(model.parent(trip), top);
        QCOMPARE(trip.data(SizeTreeModel::DepthRole).toInt(), 1);

        QVERIFY(!model.indexFor(t.drive).isValid());
        QVERIFY(!model.indexFor(t.bin).isValid());
        QVERIFY(!model.indexFor(t.bin->children.front().get()).isValid());
    }

    void invisibleRootMeansNoScope()
    {
        const Tree t = makeTree();
        SizeTreeModel model;
        model.setSnapshot(t.snapshot);
        model.setScope(t.snapshot->root.get());
        QCOMPARE(model.scope(), nullptr);
        QCOMPARE(model.rowCount(), 2);
    }

    void newSnapshotClearsScope()
    {
        const Tree t = makeTree();
        SizeTreeModel model;
        model.setSnapshot(t.snapshot);
        model.setScope(t.photos);
        model.setSnapshot(makeTree().snapshot);
        QCOMPARE(model.scope(), nullptr);
        QCOMPARE(model.rowCount(), 2);
    }
};

QTEST_MAIN(TestSizeTreeModel)
#include "tst_sizetreemodel.moc"
