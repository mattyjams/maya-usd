//
// Copyright 2019 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "TreeModelFactory.h"

#include "USDImportDialog.h"

#include <mayaUsdUI/ui/IMayaMQtUtil.h>
#include <mayaUsdUI/ui/TreeItem.h>
#include <mayaUsdUI/ui/TreeModel.h>

#include <QtCore/QObject>
#include <QtGui/QStandardItemModel>

#include <cctype>
#include <memory>
#include <type_traits>

namespace MAYAUSD_NS_DEF {

// Ensure the TreeModelFactory is not constructible, as it is intended to be used only through
// static factory methods.
//
// While additional traits like std::is_copy_constructible or std::is_move_constructible could also
// be used, the fact that the Factory cannot be (traditionally) instantiated prevents other
// constructors and assignments operators from being useful.
static_assert(
    !std::is_constructible<TreeModelFactory>::value,
    "TreeModelFactory should not be constructible.");

/*static*/
std::unique_ptr<TreeModel> TreeModelFactory::createEmptyTreeModel(
    const IMayaMQtUtil&           mayaQtUtil,
    const ImportData*             importData,
    const USDImportDialogOptions& options,
    QObject*                      parent /*= nullptr*/)
{
    std::unique_ptr<TreeModel> treeModel(new TreeModel(mayaQtUtil, importData, options, parent));

    QStringList headerLabels({ QObject::tr(""), QObject::tr("Name"), QObject::tr("Type") });
    if (options.showVariants)
        headerLabels.append(QObject::tr("Variant Set and Variant"));

    treeModel->setHorizontalHeaderLabels(headerLabels);
    return treeModel;
} // namespace MAYAUSD_NS_DEF

/*static*/
std::unique_ptr<TreeModel> TreeModelFactory::createFromStage(
    const UsdStageRefPtr&         stage,
    const IMayaMQtUtil&           mayaQtUtil,
    const ImportData*             importData,
    const USDImportDialogOptions& options,
    QObject*                      parent,
    int*                          nbItems /*= nullptr*/
)
{
    std::unique_ptr<TreeModel> treeModel
        = createEmptyTreeModel(mayaQtUtil, importData, options, parent);

    UsdPrim rootPrim = stage->GetPseudoRoot();
    UsdPrim defPrim = stage->GetDefaultPrim();

    const int cnt = options.showRoot
        ? buildTreeHierarchy(rootPrim, defPrim, treeModel->invisibleRootItem(), options)
        : buildTreeChildren(rootPrim, defPrim, treeModel->invisibleRootItem(), options);

    if (nbItems != nullptr)
        *nbItems = cnt;

    TreeItem* item = treeModel->findPrimItem(defPrim);
    if (!item)
        item = treeModel->getFirstItem();
    if (item)
        treeModel->checkEnableItem(item);

    return treeModel;
}

/*static*/
QList<QStandardItem*> TreeModelFactory::createPrimRow(
    const UsdPrim&                prim,
    const UsdPrim&                defaultPrim,
    const USDImportDialogOptions& options)
{
    const bool isDefaultPrim = (prim == defaultPrim);
    // Cache the values to be displayed, in order to avoid querying the USD Prim too frequently
    // (despite it being cached and optimized for frequent access). Avoiding frequent conversions
    // from USD Strings to Qt Strings helps in keeping memory allocations low.
    QList<QStandardItem*> ret = { new TreeItem(prim, isDefaultPrim, TreeItem::kColumnLoad),
                                  new TreeItem(prim, isDefaultPrim, TreeItem::kColumnName),
                                  new TreeItem(prim, isDefaultPrim, TreeItem::kColumnType) };

    if (options.showVariants) {
        ret.append(new TreeItem(prim, isDefaultPrim, TreeItem::kColumnVariants));
    }
    return ret;
}

/*static*/
int TreeModelFactory::buildTreeHierarchy(
    const UsdPrim&                prim,
    const UsdPrim&                defaultPrim,
    QStandardItem*                parentItem,
    const USDImportDialogOptions& options)
{
    QList<QStandardItem*> primDataCells = createPrimRow(prim, defaultPrim, options);
    parentItem->appendRow(primDataCells);
    return 1 + buildTreeChildren(prim, defaultPrim, primDataCells.front(), options);
}

/*static*/
int TreeModelFactory::buildTreeChildren(
    const UsdPrim&                prim,
    const UsdPrim&                defaultPrim,
    QStandardItem*                parentItem,
    const USDImportDialogOptions& options)
{
    int cnt = 0;
    for (const auto& childPrim : prim.GetAllChildren())
        cnt += buildTreeHierarchy(childPrim, defaultPrim, parentItem, options);
    return cnt;
}

/*static*/
int TreeModelFactory::buildTreeHierarchy(
    const UsdPrim&                prim,
    const UsdPrim&                defaultPrim,
    QStandardItem*                parentItem,
    const USDImportDialogOptions& options,
    const unordered_sdfpath_set&  primsToIncludeInTree,
    size_t&                       insertionsRemaining)
{
    int  cnt = 0;
    bool primShouldBeIncluded
        = primsToIncludeInTree.find(prim.GetPath()) != primsToIncludeInTree.end();
    if (primShouldBeIncluded) {
        QList<QStandardItem*> primDataCells = createPrimRow(prim, defaultPrim, options);
        parentItem->appendRow(primDataCells);
        ++cnt;

        // Only continue processing additional USD Prims if all expected results have not already
        // been found:
        if (--insertionsRemaining > 0) {
            for (const auto& childPrim : prim.GetAllChildren()) {
                cnt += buildTreeHierarchy(
                    childPrim,
                    defaultPrim,
                    primDataCells.front(),
                    options,
                    primsToIncludeInTree,
                    insertionsRemaining);
            }
        }
    }
    return cnt;
}

} // namespace MAYAUSD_NS_DEF
