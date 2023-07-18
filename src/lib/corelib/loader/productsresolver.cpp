/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "productsresolver.h"

#include "itemreader.h"
#include "loaderutils.h"
#include "productresolver.h"

#include <tools/stringconstants.h>

#include <algorithm>
#include <queue>
#include <vector>

namespace qbs::Internal {

class ProductsResolver
{
public:
    ProductsResolver(LoaderState &loaderState) : m_loaderState(loaderState) {}
    void resolve();

private:
    void initialize();
    void runScheduler();
    void postProcess();

    static int dependsItemCount(const Item *item);
    static int dependsItemCount(ProductContext &product);

    LoaderState &m_loaderState;
    std::queue<std::pair<ProductContext *, int>> m_productsToHandle;
    std::vector<ProductContext *> m_finishedProducts;
};

void resolveProducts(LoaderState &loaderState)
{
    ProductsResolver(loaderState).resolve();
}

void ProductsResolver::resolve()
{
    initialize();
    runScheduler();
    postProcess();
}

void ProductsResolver::initialize()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();
    std::vector<ProductContext *> sortedProducts;
    for (ProjectContext * const projectContext : topLevelProject.projects()) {
        for (ProductContext &product : projectContext->products) {
            topLevelProject.addProductToHandle(product);
            const auto it = std::lower_bound(sortedProducts.begin(), sortedProducts.end(), product,
                                             [&product](ProductContext *p1, const ProductContext &) {
                return dependsItemCount(*p1) < dependsItemCount(product);
            });
            sortedProducts.insert(it, &product);
        }
    }
    for (ProductContext * const product : sortedProducts) {
        m_productsToHandle.emplace(product, -1);
        if (product->shadowProduct)
            m_productsToHandle.emplace(product->shadowProduct.get(), -1);
    }
}

void ProductsResolver::runScheduler()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();
    while (!m_productsToHandle.empty()) {
        const auto [product, queueSizeOnInsert] = m_productsToHandle.front();
        m_productsToHandle.pop();

        // If the queue of in-progress products has shrunk since the last time we tried handling
        // this product, there has been forward progress and we can allow a deferral.
        const Deferral deferral = queueSizeOnInsert == -1
                                          || queueSizeOnInsert > int(m_productsToHandle.size())
                                      ? Deferral::Allowed : Deferral::NotAllowed;

        // If we already know that there's a dependency to an unresolved product pending,
        // we don't start up the machinery, but put the product back into the queue right away.
        if (deferral == Deferral::Allowed && product->hasDependencyToUnresolvedProduct()) {
            m_productsToHandle.emplace(product, int(m_productsToHandle.size()));
            continue;
        }

        m_loaderState.itemReader().setExtraSearchPathsStack(product->project->searchPathsStack);
        resolveProduct(*product, deferral, m_loaderState);
        if (topLevelProject.isCanceled())
            throw CancelException();

        // The search paths stack can change during dependency resolution (due to module providers);
        // check that we've rolled back all the changes
        QBS_CHECK(m_loaderState.itemReader().extraSearchPathsStack()
                  == product->project->searchPathsStack);

        // If we encountered a dependency to an in-progress product or to a bulk dependency,
        // we defer handling this product if it hasn't failed yet and there is still
        // forward progress.
        if (product->dependenciesResolvingPending()) {
            m_productsToHandle.emplace(product, int(m_productsToHandle.size()));
            topLevelProject.incProductDeferrals();
        } else {
            topLevelProject.removeProductToHandle(*product);
            if (!product->name.startsWith(StringConstants::shadowProductPrefix()))
                m_finishedProducts.push_back(product);
            topLevelProject.timingData() += product->timingData;
        }
    }
}

void ProductsResolver::postProcess()
{
    // This has to be done at the end, because we need both product and shadow product to be
    // ready, and contrary to what one might assume, there is no proper ordering between them
    // regarding dependency resolving.
    for (ProductContext * const product : m_finishedProducts)
        setupExports(*product, m_loaderState);
}

int ProductsResolver::dependsItemCount(const Item *item)
{
    int count = 0;
    for (const Item * const child : item->children()) {
        if (child->type() == ItemType::Depends)
            ++count;
    }
    return count;
}

int ProductsResolver::dependsItemCount(ProductContext &product)
{
    if (product.dependsItemCount == -1)
        product.dependsItemCount = dependsItemCount(product.item);
    return product.dependsItemCount;
}

} // namespace qbs::Internal
