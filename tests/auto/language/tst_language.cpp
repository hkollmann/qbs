/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "tst_language.h"

#include <language/identifiersearch.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/scripttools.h>
#include <tools/error.h>

TestLanguage::TestLanguage() : m_settings(qbsSettings())
{
    defaultParameters.buildRoot = "/some/build/directory";
}

TestLanguage::~TestLanguage()
{
}

QHash<QString, ResolvedProductPtr> TestLanguage::productsFromProject(ResolvedProjectPtr project)
{
    QHash<QString, ResolvedProductPtr> result;
    foreach (ResolvedProductPtr product, project->products)
        result.insert(product->name, product);
    return result;
}

ResolvedModuleConstPtr TestLanguage::findModuleByName(ResolvedProductPtr product, const QString &name)
{
    foreach (const ResolvedModuleConstPtr &module, product->modules)
        if (module->name == name)
            return module;
    return ResolvedModuleConstPtr();
}

QVariant TestLanguage::productPropertyValue(ResolvedProductPtr product, QString propertyName)
{
    QStringList propertyNameComponents = propertyName.split(QLatin1Char('.'));
    if (propertyNameComponents.count() > 1)
        propertyNameComponents.prepend(QLatin1String("modules"));
    return getConfigProperty(product->properties->value(), propertyNameComponents);
}

void TestLanguage::initTestCase()
{
    //Logger::instance().setLogSink(new ConsolePrintLogSink);
    //Logger::instance().setLevel(LoggerTrace);
    ScriptEngine *engine = new ScriptEngine(this);
    loader = new Loader(engine, m_settings.data());
    loader->setSearchPaths(QStringList()
                           << QLatin1String(SRCDIR "../../../share/qbs")
                           << QLatin1String(SRCDIR "testdata"));
    setConfigProperty(defaultParameters.buildConfiguration,
                      QStringList() << "qbs" << "targetOS", "linux");
}

void TestLanguage::cleanupTestCase()
{
    delete loader;
}

void TestLanguage::conditionalDepends()
{
    bool exceptionCaught = false;
    ResolvedProductPtr product;
    ResolvedModuleConstPtr dependency;
    try {
        defaultParameters.projectFilePath = SRCDIR "testdata/conditionaldepends.qbs";
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);

        product = products.value("conditionaldepends_derived");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("conditionaldepends_derived_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("product_props_true");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("product_props_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("project_props_true");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("project_props_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("module_props_true");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy2");
        QVERIFY(dependency);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("module_props_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy2");
        QVERIFY(dependency);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("contradictory_conditions1");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("contradictory_conditions2");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);
    } catch (const Error &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::invalidDepends_data()
{
    QTest::addColumn<QString>("errorMessage");
    QTest::newRow("unknown_module")
            << "Product dependency 'neithermodulenorproduct' not found";
    QTest::newRow("submodule_syntax")
            << "Depends.submodules cannot be used if name contains a dot";
}

void TestLanguage::invalidDepends()
{
    QFETCH(QString, errorMessage);
    QString fileName = QString::fromLocal8Bit(QTest::currentDataTag()) + QLatin1String(".qbs");
    try {
        defaultParameters.projectFilePath
                = QString::fromLatin1(SRCDIR "testdata/invaliddepends/") + fileName;
        loader->loadProject(defaultParameters);
    } catch (const Error &e) {
        if (!e.toString().contains(errorMessage)) {
            qDebug() << "Message:  " << e.toString();
            qDebug() << "Expected: " << errorMessage;
            QFAIL("Unexpected error message.");
        }
        return;
    }
    QFAIL("No error thrown on invalid input.");
}

void TestLanguage::groupName()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.projectFilePath = SRCDIR "testdata/groupname.qbs";
        ResolvedProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 2);

        ResolvedProductPtr product = products.value("MyProduct");
        QVERIFY(product);
        QCOMPARE(product->groups.count(), 2);
        GroupConstPtr group = product->groups.at(0);
        QVERIFY(group);
        QCOMPARE(group->name, QString("MyProduct"));
        group = product->groups.at(1);
        QVERIFY(group);
        QCOMPARE(group->name, QString("MyProduct.MyGroup"));

        product = products.value("My2ndProduct");
        QVERIFY(product);
        QCOMPARE(product->groups.count(), 3);
        group = product->groups.at(0);
        QVERIFY(group);
        QCOMPARE(group->name, QString("My2ndProduct"));
        group = product->groups.at(1);
        QVERIFY(group);
        QCOMPARE(group->name, QString("My2ndProduct.MyGroup"));
        group = product->groups.at(2);
        QVERIFY(group);
        QCOMPARE(group->name, QString("Group 2"));
    }
    catch (const Error &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::identifierSearch_data()
{
    QTest::addColumn<bool>("expectedHasNarf");
    QTest::addColumn<bool>("expectedHasZort");
    QTest::addColumn<QString>("sourceCode");
    QTest::newRow("no narf, no zort") << false << false << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'bar';\n"
                                  "        print(foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("narf, no zort") << true << false << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'zort';\n"
                                  "        print(narf + foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("no narf, zort") << false << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'narf';\n"
                                  "        print(zort + foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("narf, zort") << true << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = narf;\n"
                                  "        foo = foo + zort;\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("2 narfs, 1 zort") << true << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = narf;\n"
                                  "        foo = narf + foo + zort;\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
}

void TestLanguage::identifierSearch()
{
    QFETCH(bool, expectedHasNarf);
    QFETCH(bool, expectedHasZort);
    QFETCH(QString, sourceCode);

    bool hasNarf = !expectedHasNarf;
    bool hasZort = !expectedHasZort;
    IdentifierSearch isearch;
    isearch.add("narf", &hasNarf);
    isearch.add("zort", &hasZort);

    QbsQmlJS::Engine engine;
    QbsQmlJS::Lexer lexer(&engine);
    lexer.setCode(sourceCode, 1);
    QbsQmlJS::Parser parser(&engine);
    QVERIFY(parser.parse());
    QVERIFY(parser.ast());
    isearch.start(parser.ast());
    QCOMPARE(hasNarf, expectedHasNarf);
    QCOMPARE(hasZort, expectedHasZort);
}

void TestLanguage::jsImportUsedInMultipleScopes_data()
{
    QTest::addColumn<QString>("buildVariant");
    QTest::addColumn<QString>("expectedProductName");
    QTest::newRow("debug") << QString("debug") << QString("MyProduct_debug");
    QTest::newRow("release") << QString("release") << QString("MyProduct");
}

void TestLanguage::jsImportUsedInMultipleScopes()
{
    QFETCH(QString, buildVariant);
    QFETCH(QString, expectedProductName);

    bool exceptionCaught = false;
    try {
        QVariantMap customBuildConfig = defaultParameters.buildConfiguration;
        setConfigProperty(customBuildConfig, QStringList() << "qbs" << "buildVariant",
                          buildVariant);
        SetupProjectParameters params = defaultParameters;
        params.projectFilePath = SRCDIR "testdata/jsimportsinmultiplescopes.qbs";
        params.buildConfiguration = customBuildConfig;
        ResolvedProjectPtr project = loader->loadProject(params);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);
        ResolvedProductPtr product = products.values().first();
        QVERIFY(product);
        QCOMPARE(product->name, expectedProductName);
    }
    catch (const Error &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::modules_data()
{
    QTest::addColumn<QStringList>("expectedModulesInProduct");
    QTest::newRow("init") << QStringList();
    QTest::newRow("no_modules")
            << (QStringList() << "qbs");
    QTest::newRow("qt_core")
            << (QStringList() << "qbs" << "dummyqt/core");
    QTest::newRow("qt_gui")
            << (QStringList() << "qbs" << "dummyqt/core" << "dummyqt/gui");
    QTest::newRow("qt_gui_network")
            << (QStringList() << "qbs" << "dummyqt/core" << "dummyqt/gui" << "dummyqt/network");
    QTest::newRow("cleanup") << QStringList();
}

void TestLanguage::modules()
{
    QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    if (productName == "init") {
        bool exceptionCaught = false;
        try {
            defaultParameters.projectFilePath = SRCDIR "testdata/modules.qbs";
            project = loader->loadProject(defaultParameters);
            QVERIFY(project);
        } catch (const Error &e) {
            exceptionCaught = true;
            qDebug() << e.toString();
        }
        QCOMPARE(exceptionCaught, false);
        return;
    } else if (productName == "cleanup") {
        project.clear();
        return;
    }

    QVERIFY(project);
    QFETCH(QStringList, expectedModulesInProduct);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(product);
    QCOMPARE(product->name, productName);
    QStringList modulesInProduct;
    foreach (ResolvedModuleConstPtr m, product->modules)
        modulesInProduct += m->name;
    modulesInProduct.sort();
    expectedModulesInProduct.sort();
    QCOMPARE(modulesInProduct, expectedModulesInProduct);
}

void TestLanguage::outerInGroup()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.projectFilePath = SRCDIR "testdata/outerInGroup.qbs";
        ResolvedProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);
        ResolvedProductPtr product = products.value("OuterInGroup");
        QVERIFY(product);
        QCOMPARE(product->groups.count(), 2);
        GroupPtr group = product->groups.at(0);
        QVERIFY(group);
        QCOMPARE(group->name, product->name);
        QCOMPARE(group->files.count(), 1);
        SourceArtifactConstPtr artifact = group->files.first();
        QVariant installDir = artifact->properties->qbsPropertyValue("installDir");
        QCOMPARE(installDir.toString(), QString("/somewhere"));
        group = product->groups.at(1);
        QVERIFY(group);
        QCOMPARE(group->name, QString("Special Group"));
        QCOMPARE(group->files.count(), 1);
        artifact = group->files.first();
        installDir = artifact->properties->qbsPropertyValue("installDir");
        QCOMPARE(installDir.toString(), QString("/somewhere/else"));
    }
    catch (const Error &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::productConditions()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.projectFilePath = SRCDIR "testdata/productconditions.qbs";
        ResolvedProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 4);
        ResolvedProductPtr product;
        product = products.value("product_no_condition");
        QVERIFY(product);
        QVERIFY(product->enabled);

        product = products.value("product_true_condition");
        QVERIFY(product);
        QVERIFY(product->enabled);

        product = products.value("product_condition_dependent_of_module");
        QVERIFY(product);
        QVERIFY(product->enabled);

        product = products.value("product_false_condition");
        QVERIFY(product);
        QVERIFY(!product->enabled);
    }
    catch (const Error &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::propertiesBlocks_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QStringList>("expectedValues");

    QTest::newRow("init") << QString() << QStringList();
    QTest::newRow("property_overwrite") << QString("dummy.defines") << QStringList("OVERWRITTEN");
    QTest::newRow("property_overwrite_no_outer") << QString("dummy.defines") << QStringList("OVERWRITTEN");
    QTest::newRow("property_append_to_outer") << QString("dummy.defines") << (QStringList() << QString("ONE") << QString("TWO"));

    QTest::newRow("multiple_exclusive_properties") << QString("dummy.defines") << QStringList("OVERWRITTEN");
    QTest::newRow("multiple_exclusive_properties_no_outer") << QString("dummy.defines") << QStringList("OVERWRITTEN");
    QTest::newRow("multiple_exclusive_properties_append_to_outer") << QString("dummy.defines") << (QStringList() << QString("ONE") << QString("TWO"));

    QTest::newRow("ambiguous_properties") << QString("dummy.defines") << (QStringList() << QString("ONE") << QString("TWO") << QString("THREE"));
    QTest::newRow("cleanup") << QString() << QStringList();
}

void TestLanguage::propertiesBlocks()
{
    QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    if (productName == "init") {
        bool exceptionCaught = false;
        try {
            defaultParameters.projectFilePath = SRCDIR "testdata/propertiesblocks.qbs";
            project = loader->loadProject(defaultParameters);
            QVERIFY(project);
        } catch (const Error &e) {
            exceptionCaught = true;
            qDebug() << e.toString();
        }
        QCOMPARE(exceptionCaught, false);
        return;
    } else if (productName == "cleanup") {
        project = ResolvedProjectPtr();
        return;
    }

    QFETCH(QString, propertyName);
    QFETCH(QStringList, expectedValues);
    QVERIFY(project);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(product);
    QCOMPARE(product->name, productName);
    QVariant v = productPropertyValue(product, propertyName);
    QCOMPARE(v.toStringList(), expectedValues);
}

void TestLanguage::fileTags_data()
{
    QTest::addColumn<int>("numberOfGroups");
    QTest::addColumn<QStringList>("expectedFileTags");

    QTest::newRow("init") << 0 << QStringList();
    QTest::newRow("filetagger_project_scope") << 1 << (QStringList() << "cpp");
    QTest::newRow("filetagger_product_scope") << 1 << (QStringList() << "asm");
    QTest::newRow("unknown_file_tag") << 1 << (QStringList() << "unknown-file-tag");
    QTest::newRow("set_file_tag_via_group") << 2 << (QStringList() << "c++");
    QTest::newRow("override_file_tag_via_group") << 2 << (QStringList() << "c++");
    QTest::newRow("add_file_tag_via_group") << 2 << (QStringList() << "cpp" << "zzz");
    QTest::newRow("add_file_tag_via_group_and_file_ref") << 2 << (QStringList() << "cpp" << "zzz");
    QTest::newRow("cleanup") << 0 << QStringList();
}

void TestLanguage::fileTags()
{
    QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    if (productName == QLatin1String("init")) {
        bool exceptionCaught = false;
        try {
            defaultParameters.projectFilePath = SRCDIR "testdata/filetags.qbs";
            project = loader->loadProject(defaultParameters);
            QVERIFY(project);
        }
        catch (const Error &e) {
            exceptionCaught = true;
            qDebug() << e.toString();
        }
        QCOMPARE(exceptionCaught, false);
        return;
    } else if (productName == QLatin1String("cleanup")) {
        project = ResolvedProjectPtr();
        return;
    }

    QVERIFY(project);
    QFETCH(int, numberOfGroups);
    QFETCH(QStringList, expectedFileTags);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    ResolvedProductPtr product;
    QVERIFY(product = products.value(productName));
    QCOMPARE(product->groups.count(), numberOfGroups);
    GroupPtr group = product->groups.last();
    QVERIFY(group);
    QCOMPARE(group->files.count(), 1);
    SourceArtifactConstPtr sourceFile = group->files.first();
    QStringList fileTags = sourceFile->fileTags.toList();
    fileTags.sort();
    QCOMPARE(fileTags, expectedFileTags);
}

void TestLanguage::wildcards_data()
{
    QTest::addColumn<bool>("useGroup");
    QTest::addColumn<QStringList>("filesToCreate");
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<QStringList>("patterns");
    QTest::addColumn<QStringList>("excludePatterns");
    QTest::addColumn<QStringList>("expected");

    const bool useGroup = true;

    for (int i = 0; i <= 1; ++i) {
        const bool useGroup = i;
        const QByteArray dataTagSuffix = useGroup ? " group" : " nogroup";
        QTest::newRow(QByteArray("simple 1") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << (QStringList() << "*.h")
                << QStringList()
                << (QStringList() << "foo.h" << "bar.h");
        QTest::newRow(QByteArray("simple 2") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << (QStringList() << "foo.*")
                << QStringList()
                << (QStringList() << "foo.h" << "foo.cpp");
        QTest::newRow(QByteArray("simple 3") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << (QStringList() << "*.h" << "*.cpp")
                << QStringList()
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp");
        QTest::newRow(QByteArray("exclude 1") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << (QStringList() << "*.h" << "*.cpp")
                << (QStringList() << "bar*")
                << (QStringList() << "foo.h" << "foo.cpp");
        QTest::newRow(QByteArray("exclude 2") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << (QStringList() << "*")
                << (QStringList() << "*.qbs")
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp");
        QTest::newRow(QByteArray("non-recursive") + dataTagSuffix)
                << useGroup
                << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp")
                << QString()
                << (QStringList() << "a/*")
                << QStringList()
                << (QStringList() << "a/foo.h" << "a/foo.cpp");
    }
    QTest::newRow(QByteArray("recursive 1"))
            << useGroup
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp")
            << QString()
            << (QStringList() << "a/**")
            << QStringList()
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp");
    QTest::newRow(QByteArray("recursive 2"))
            << useGroup
            << (QStringList()
                << "d/1.h" << "b/d/1.h" << "b/c/d/1.h"
                << "d/e/1.h" << "b/d/e/1.h" << "b/c/d/e/1.h"
                << "a/d/1.h" << "a/b/d/1.h" << "a/b/c/d/1.h"
                << "a/d/e/1.h" << "a/b/d/e/1.h" << "a/b/c/d/e/1.h"
                << "a/d/1.cpp" << "a/b/d/1.cpp" << "a/b/c/d/1.h"
                << "a/d/e/1.cpp" << "a/b/d/e/1.cpp" << "a/b/c/d/e/1.cpp")
            << QString()
            << (QStringList() << "a/**/d/*.h")
            << QStringList()
            << (QStringList() << "a/d/1.h" << "a/b/d/1.h" << "a/b/c/d/1.h");
    QTest::newRow(QByteArray("recursive 3"))
            << useGroup
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp")
            << QString()
            << (QStringList() << "a/**/**/**")
            << QStringList()
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp");
    QTest::newRow(QByteArray("prefix"))
            << useGroup
            << (QStringList() << "subdir/foo.h" << "subdir/foo.cpp" << "subdir/bar.h"
                << "subdir/bar.cpp")
            << QString("subdir/")
            << (QStringList() << "*.h")
            << QStringList()
            << (QStringList() << "subdir/foo.h" << "subdir/bar.h");
}

void TestLanguage::wildcards()
{
    QFETCH(bool, useGroup);
    QFETCH(QStringList, filesToCreate);
    QFETCH(QString, prefix);
    QFETCH(QStringList, patterns);
    QFETCH(QStringList, excludePatterns);
    QFETCH(QStringList, expected);

    // create test directory
    const QString wildcardsTestDir = QDir::currentPath() + "/_wildcards_test_dir_";
    {
        QString errorMessage;
        if (QFile::exists(wildcardsTestDir)) {
            if (!removeDirectoryWithContents(wildcardsTestDir, &errorMessage)) {
                qDebug() << errorMessage;
                QVERIFY2(false, "removeDirectoryWithContents failed");
            }
        }
        QDir().mkdir(wildcardsTestDir);
    }

    // create project file
    const QString groupName = "Keks";
    QString dataTag = QString::fromLocal8Bit(QTest::currentDataTag());
    dataTag.replace(' ', '_');
    const QString projectFilePath = wildcardsTestDir + "/test_" + dataTag + ".qbs";
    {
        QFile projectFile(projectFilePath);
        QVERIFY(projectFile.open(QIODevice::WriteOnly));
        QTextStream s(&projectFile);
        s << "import qbs.base 1.0" << endl << endl
          << "Application {" << endl
          << "  name: \"MyProduct\"" << endl;
        if (useGroup) {
            s << "  Group {" << endl
              << "     name: " << toJSLiteral(groupName) << endl;
        }
        if (!prefix.isEmpty())
            s << "  prefix: " << toJSLiteral(prefix) << endl;
        if (!patterns.isEmpty())
            s << "  files: " << toJSLiteral(patterns) << endl;
        if (!excludePatterns.isEmpty())
            s << "  excludeFiles: " << toJSLiteral(excludePatterns) << endl;
        if (useGroup)
            s << "  }" << endl;
        s << "}" << endl << endl;
    }

    // create files
    foreach (QString filePath, filesToCreate) {
        filePath.prepend(wildcardsTestDir + '/');
        QFileInfo fi(filePath);
        if (!QDir(fi.path()).exists())
            QVERIFY(QDir().mkpath(fi.path()));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    // read the project
    bool exceptionCaught = false;
    ResolvedProductPtr product;
    try {
        defaultParameters.projectFilePath = projectFilePath;
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        product = products.value("MyProduct");
        QVERIFY(product);
        GroupPtr group;
        if (useGroup) {
            QCOMPARE(product->groups.count(), 2);
            foreach (const GroupPtr &rg, product->groups) {
                if (rg->name == groupName) {
                    group = rg;
                    break;
                }
            }
        } else {
            QCOMPARE(product->groups.count(), 1);
            group = product->groups.first();
        }
        QVERIFY(group);
        QCOMPARE(group->files.count(), 0);
        SourceWildCards::Ptr wildcards = group->wildcards;
        QVERIFY(wildcards);
        QStringList actualFilePaths;
        foreach (const SourceArtifactConstPtr &artifact, wildcards->files) {
            QString str = artifact->absoluteFilePath;
            int idx = str.indexOf(wildcardsTestDir);
            if (idx != -1)
                str.remove(0, idx + wildcardsTestDir.count() + 1);
            actualFilePaths << str;
        }
        actualFilePaths.sort();
        expected.sort();
        QCOMPARE(actualFilePaths, expected);
    } catch (const Error &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

QTEST_MAIN(TestLanguage)
