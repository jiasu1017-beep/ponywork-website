// 应用推荐管理模块
(function() {
    let apps = [];
    let editingAppId = null;

    // 获取认证 token
    function getAuthToken() {
        return localStorage.getItem('adminToken');
    }

    // 加载应用列表
    async function loadApps(page = 1) {
        const search = document.getElementById('appSearch')?.value || '';

        try {
            const res = await fetch(`/api/admin/recommended-apps?page=${page}&limit=20&search=${encodeURIComponent(search)}`, {
                headers: { 'Authorization': 'Bearer ' + getAuthToken() }
            });
            const data = await res.json();

            if (data.success) {
                apps = data.apps || [];
                renderTable();
                renderPagination('appsPagination', data.pagination, loadApps);
            } else {
                showToast(data.message || '加载应用列表失败', 'danger');
            }
        } catch (err) {
            console.error('加载应用列表错误:', err);
            showToast('加载应用列表失败', 'danger');
        }
    }

    // 渲染表格
    function renderTable() {
        const tbody = document.getElementById('appsTableBody');
        if (!apps || apps.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" class="text-center text-muted">暂无数据</td></tr>';
            return;
        }

        tbody.innerHTML = apps.map(app => `
            <tr>
                <td>${app.id}</td>
                <td><strong>${escapeHtml(app.name)}</strong></td>
                <td>${escapeHtml(app.category || '-')}</td>
                <td>${app.downloads ? app.downloads.length : 0}</td>
                <td>
                    <span class="badge badge-${app.is_enabled ? 'active' : 'inactive'}">
                        ${app.is_enabled ? '启用' : '禁用'}
                    </span>
                </td>
                <td>${app.sort_order || 0}</td>
                <td>
                    <button class="btn btn-sm btn-primary btn-action" onclick="RecommendedApps.edit(${app.id})" title="编辑">
                        <i class="bi bi-pencil"></i>
                    </button>
                    <button class="btn btn-sm btn-info btn-action" onclick="RecommendedApps.manageDownloads(${app.id})" title="下载地址">
                        <i class="bi bi-link"></i>
                    </button>
                    <button class="btn btn-sm btn-danger btn-action" onclick="RecommendedApps.delete(${app.id})" title="删除">
                        <i class="bi bi-trash"></i>
                    </button>
                </td>
            </tr>
        `).join('');
    }

    // 显示新增/编辑模态框
    function showEditModal(app) {
        const isEdit = app && app.id;
        const modalId = 'editAppModal';

        let existingModal = document.getElementById(modalId);
        if (existingModal) {
            existingModal.remove();
        }

        const modalHtml = `
            <div class="modal fade" id="${modalId}" tabindex="-1">
                <div class="modal-dialog">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h5 class="modal-title">${isEdit ? '编辑应用' : '新增应用'}</h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body">
                            <input type="hidden" id="editAppId" value="${app ? app.id : ''}">
                            <div class="mb-3">
                                <label class="form-label">应用名称</label>
                                <input type="text" class="form-control" id="editAppName" value="${app ? escapeHtml(app.name) : ''}" required>
                            </div>
                            <div class="mb-3">
                                <label class="form-label">分类</label>
                                <input type="text" class="form-control" id="editAppCategory" value="${app ? escapeHtml(app.category || '') : ''}" placeholder="如：效率工具、开发工具">
                            </div>
                            <div class="mb-3">
                                <label class="form-label">描述</label>
                                <textarea class="form-control" id="editAppDescription" rows="3" placeholder="应用简介">${app ? escapeHtml(app.description || '') : ''}</textarea>
                            </div>
                            <div class="mb-3">
                                <label class="form-label">图标URL</label>
                                <input type="text" class="form-control" id="editAppIcon" value="${app ? escapeHtml(app.icon_url || '') : ''}" placeholder="https://...">
                            </div>
                            <div class="mb-3">
                                <label class="form-label">排序</label>
                                <input type="number" class="form-control" id="editAppSortOrder" value="${app ? app.sort_order : 0}" min="0">
                            </div>
                            <div class="mb-3">
                                <label class="form-label">状态</label>
                                <select class="form-select" id="editAppStatus">
                                    <option value="1" ${app && app.is_enabled ? 'selected' : ''}>启用</option>
                                    <option value="0" ${!app || !app.is_enabled ? 'selected' : ''}>禁用</option>
                                </select>
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">取消</button>
                            <button type="button" class="btn btn-primary" onclick="RecommendedApps.save()">保存</button>
                        </div>
                    </div>
                </div>
            </div>
        `;

        document.body.insertAdjacentHTML('beforeend', modalHtml);
        const modal = new bootstrap.Modal(document.getElementById(modalId));
        modal.show();

        document.getElementById(modalId).addEventListener('hidden.bs.modal', function() {
            this.remove();
        });
    }

    // 保存应用
    async function saveApp() {
        const id = document.getElementById('editAppId').value;
        const name = document.getElementById('editAppName').value;
        const category = document.getElementById('editAppCategory').value;
        const description = document.getElementById('editAppDescription').value;
        const icon_url = document.getElementById('editAppIcon').value;
        const sort_order = parseInt(document.getElementById('editAppSortOrder').value) || 0;
        const is_enabled = document.getElementById('editAppStatus').value === '1';

        if (!name) {
            showToast('应用名称不能为空', 'danger');
            return;
        }

        const method = id ? 'PUT' : 'POST';
        const url = id ? `/api/admin/recommended-apps/${id}` : '/api/admin/recommended-apps';
        const body = { name, category, description, icon_url, sort_order, is_enabled };

        try {
            const res = await fetch(url, {
                method: method,
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': 'Bearer ' + getAuthToken()
                },
                body: JSON.stringify(body)
            });
            const data = await res.json();

            if (data.success) {
                showToast(id ? '应用更新成功' : '应用创建成功', 'success');
                bootstrap.Modal.getInstance(document.getElementById('editAppModal'))?.hide();
                loadApps();
            } else {
                showToast(data.message || '保存失败', 'danger');
            }
        } catch (err) {
            console.error('保存应用错误:', err);
            showToast('保存失败', 'danger');
        }
    }

    // 删除应用
    async function deleteApp(id) {
        if (!confirm('确定要删除这个应用吗？')) return;

        try {
            const res = await fetch(`/api/admin/recommended-apps/${id}`, {
                method: 'DELETE',
                headers: { 'Authorization': 'Bearer ' + getAuthToken() }
            });
            const data = await res.json();

            if (data.success) {
                showToast('删除成功', 'success');
                loadApps();
            } else {
                showToast(data.message || '删除失败', 'danger');
            }
        } catch (err) {
            showToast('删除失败', 'danger');
        }
    }

    // 管理下载地址
    function showDownloadsModal(appId) {
        const app = apps.find(a => a.id === appId);
        if (!app) return;

        const modalId = 'downloadsModal';
        let existingModal = document.getElementById(modalId);
        if (existingModal) {
            existingModal.remove();
        }

        const downloads = app.downloads || [];
        const downloadsHtml = downloads.length > 0
            ? downloads.map((dl, index) => `
                <tr>
                    <td>${index + 1}</td>
                    <td>${escapeHtml(dl.name || '-')}</td>
                    <td><a href="${escapeHtml(dl.url)}" target="_blank" class="text-break">${escapeHtml(dl.url)}</a></td>
                    <td>
                        <button class="btn btn-sm btn-danger btn-action" onclick="RecommendedApps.deleteDownload(${appId}, ${dl.id})">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                </tr>
            `).join('')
            : '<tr><td colspan="4" class="text-center text-muted">暂无下载地址</td></tr>';

        const modalHtml = `
            <div class="modal fade" id="${modalId}" tabindex="-1">
                <div class="modal-dialog modal-lg">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h5 class="modal-title">管理下载地址 - ${escapeHtml(app.name)}</h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body">
                            <div class="mb-3">
                                <h6>下载地址列表</h6>
                                <table class="table table-sm table-hover">
                                    <thead>
                                        <tr>
                                            <th>#</th>
                                            <th>名称</th>
                                            <th>URL</th>
                                            <th>操作</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        ${downloadsHtml}
                                    </tbody>
                                </table>
                            </div>
                            <hr>
                            <h6>添加新地址</h6>
                            <div class="row">
                                <div class="col-md-4 mb-2">
                                    <input type="text" class="form-control" id="newDownloadName" placeholder="名称（如：官网下载）">
                                </div>
                                <div class="col-md-6 mb-2">
                                    <input type="text" class="form-control" id="newDownloadUrl" placeholder="下载地址URL">
                                </div>
                                <div class="col-md-2 mb-2">
                                    <button class="btn btn-primary w-100" onclick="RecommendedApps.addDownload(${appId})">添加</button>
                                </div>
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">关闭</button>
                        </div>
                    </div>
                </div>
            </div>
        `;

        document.body.insertAdjacentHTML('beforeend', modalHtml);
        const modal = new bootstrap.Modal(document.getElementById(modalId));
        modal.show();

        document.getElementById(modalId).addEventListener('hidden.bs.modal', function() {
            this.remove();
        });
    }

    // 添加下载地址
    async function addDownload(appId) {
        const name = document.getElementById('newDownloadName').value;
        const url = document.getElementById('newDownloadUrl').value;

        if (!url) {
            showToast('下载地址URL不能为空', 'danger');
            return;
        }

        try {
            const res = await fetch(`/api/admin/recommended-apps/${appId}/downloads`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': 'Bearer ' + getAuthToken()
                },
                body: JSON.stringify({ name, url })
            });
            const data = await res.json();

            if (data.success) {
                showToast('添加成功', 'success');
                showDownloadsModal(appId);
            } else {
                showToast(data.message || '添加失败', 'danger');
            }
        } catch (err) {
            showToast('添加失败', 'danger');
        }
    }

    // 删除下载地址
    async function deleteDownload(appId, downloadId) {
        if (!confirm('确定要删除这个下载地址吗？')) return;

        try {
            const res = await fetch(`/api/admin/recommended-apps/${appId}/downloads/${downloadId}`, {
                method: 'DELETE',
                headers: { 'Authorization': 'Bearer ' + getAuthToken() }
            });
            const data = await res.json();

            if (data.success) {
                showToast('删除成功', 'success');
                showDownloadsModal(appId);
            } else {
                showToast(data.message || '删除失败', 'danger');
            }
        } catch (err) {
            showToast('删除失败', 'danger');
        }
    }

    // 辅助函数
    function escapeHtml(text) {
        if (!text) return '';
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    function showToast(message, type = 'info') {
        const toast = document.createElement('div');
        toast.className = `toast align-items-center text-white bg-${type} border-0`;
        toast.setAttribute('role', 'alert');
        toast.setAttribute('aria-live', 'assertive');
        toast.setAttribute('aria-atomic', 'true');
        toast.innerHTML = `
            <div class="d-flex">
                <div class="toast-body">${message}</div>
                <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast"></button>
            </div>
        `;
        document.querySelector('.toast-container').appendChild(toast);
        const bsToast = new bootstrap.Toast(toast);
        bsToast.show();
        setTimeout(() => toast.remove(), 5000);
    }

    function renderPagination(elementId, pagination, callback) {
        const el = document.getElementById(elementId);
        if (!el || !pagination || pagination.pages <= 1) {
            if (el) el.innerHTML = '';
            return;
        }

        let html = '<nav><ul class="pagination mb-0">';

        // 首页
        html += `<li class="page-item ${pagination.page === 1 ? 'disabled' : ''}">
            <a class="page-link" href="#" onclick="event.preventDefault(); if(${pagination.page} > 1) RecommendedApps.loadPage(1)">首页</a>
        </li>`;

        // 上一页
        html += `<li class="page-item ${pagination.page === 1 ? 'disabled' : ''}">
            <a class="page-link" href="#" onclick="event.preventDefault(); if(${pagination.page} > 1) RecommendedApps.loadPage(${pagination.page - 1})">上一页</a>
        </li>`;

        // 页码
        for (let i = 1; i <= pagination.pages; i++) {
            if (i === 1 || i === pagination.pages || (i >= pagination.page - 2 && i <= pagination.page + 2)) {
                html += `<li class="page-item ${i === pagination.page ? 'active' : ''}">
                    <a class="page-link" href="#" onclick="event.preventDefault(); RecommendedApps.loadPage(${i})">${i}</a>
                </li>`;
            } else if (i === pagination.page - 3 || i === pagination.page + 3) {
                html += '<li class="page-item disabled"><span class="page-link">...</span></li>';
            }
        }

        // 下一页
        html += `<li class="page-item ${pagination.page === pagination.pages ? 'disabled' : ''}">
            <a class="page-link" href="#" onclick="event.preventDefault(); if(${pagination.page} < ${pagination.pages}) RecommendedApps.loadPage(${pagination.page + 1})">下一页</a>
        </li>`;

        // 末页
        html += `<li class="page-item ${pagination.page === pagination.pages ? 'disabled' : ''}">
            <a class="page-link" href="#" onclick="event.preventDefault(); if(${pagination.page} < ${pagination.pages}) RecommendedApps.loadPage(${pagination.pages})">末页</a>
        </li>`;

        html += '</ul></nav>';
        el.innerHTML = html;
    }

    // 暴露全局接口
    window.RecommendedApps = {
        load: loadApps,
        loadPage: loadApps,
        showAddModal: () => showEditModal(null),
        edit: (id) => {
            const app = apps.find(a => a.id === id);
            if (app) showEditModal(app);
        },
        save: saveApp,
        delete: deleteApp,
        manageDownloads: showDownloadsModal,
        addDownload: addDownload,
        deleteDownload: deleteDownload
    };

    // 搜索事件绑定
    document.addEventListener('DOMContentLoaded', function() {
        const searchInput = document.getElementById('appSearch');
        if (searchInput) {
            searchInput.addEventListener('input', debounce(() => loadApps(1), 500));
        }
    });

    function debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }
})();