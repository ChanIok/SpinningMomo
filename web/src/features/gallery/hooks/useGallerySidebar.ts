import { useState, useCallback, useMemo, useEffect } from 'react'
import { useAssetsStore } from '@/lib/assets/assetsStore'
import { mockFolders, mockTags } from '@/lib/assets/mockData'
import type { Folder, Tag } from '@/lib/assets/mockData'

interface UseGallerySidebarOptions {
  useMockData?: boolean
}

export function useGallerySidebar(options: UseGallerySidebarOptions = {}) {
  const { useMockData = true } = options

  // 获取 store 状态
  const { sidebar, setSidebarActiveSection } = useAssetsStore()

  // 本地选择状态
  const [selectedFolder, setSelectedFolder] = useState<string | null>(null)
  const [selectedTag, setSelectedTag] = useState<string | null>(null)
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set())

  // 数据获取 - 目前使用 mock 数据，将来可以扩展为真实 API 调用
  const folders = useMemo(() => {
    if (useMockData) {
      return mockFolders
    }
    // 将来可以从 API 获取
    return []
  }, [useMockData])

  const tags = useMemo(() => {
    if (useMockData) {
      return mockTags
    }
    // 将来可以从 API 获取
    return []
  }, [useMockData])

  // 从 localStorage 恢复展开状态
  useEffect(() => {
    const savedExpandedFolders = new Set<string>()

    // 递归检查所有文件夹的保存状态
    const checkFolderExpanded = (folderList: Folder[], level = 0) => {
      folderList.forEach((folder) => {
        const savedState = localStorage.getItem(`folder-expanded-${folder.id}`)
        if (savedState === 'true' || (savedState === null && level === 0)) {
          // 默认展开第一级，或者恢复保存的展开状态
          savedExpandedFolders.add(folder.id)
        }

        if (folder.children) {
          checkFolderExpanded(folder.children, level + 1)
        }
      })
    }

    checkFolderExpanded(folders)
    setExpandedFolders(savedExpandedFolders)
  }, [folders])

  // 文件夹选择
  const selectFolder = useCallback(
    (folderId: string, folderName: string) => {
      setSelectedFolder(folderId)
      setSelectedTag(null) // 清除标签选择
      setSidebarActiveSection('folders')
      console.log(`选中文件夹: ${folderName}`)

      // 这里可以触发筛选逻辑
      // 将来可以通过 assets hook 来筛选对应文件夹的资产
    },
    [setSidebarActiveSection]
  )

  // 标签选择
  const selectTag = useCallback(
    (tagId: string, tagName: string) => {
      setSelectedTag(tagId)
      setSelectedFolder(null) // 清除文件夹选择
      setSidebarActiveSection('tags')
      console.log(`选中标签: ${tagName}`)

      // 这里可以触发筛选逻辑
      // 将来可以通过 assets hook 来筛选对应标签的资产
    },
    [setSidebarActiveSection]
  )

  // 选择所有媒体
  const selectAllMedia = useCallback(() => {
    setSelectedFolder(null)
    setSelectedTag(null)
    setSidebarActiveSection('all')
    console.log('选中所有媒体')
  }, [setSidebarActiveSection])

  // 清除选择
  const clearSelection = useCallback(() => {
    setSelectedFolder(null)
    setSelectedTag(null)
    setSidebarActiveSection('all')
  }, [setSidebarActiveSection])

  // 文件夹展开/折叠
  const toggleFolderExpanded = useCallback((folderId: string) => {
    setExpandedFolders((prev) => {
      const newSet = new Set(prev)
      const isExpanded = newSet.has(folderId)

      if (isExpanded) {
        newSet.delete(folderId)
      } else {
        newSet.add(folderId)
      }

      // 保存到 localStorage
      localStorage.setItem(`folder-expanded-${folderId}`, (!isExpanded).toString())
      console.log(`文件夹 ${folderId} ${!isExpanded ? '展开' : '折叠'}`)

      return newSet
    })
  }, [])

  // 检查文件夹是否展开
  const isFolderExpanded = useCallback(
    (folderId: string) => {
      return expandedFolders.has(folderId)
    },
    [expandedFolders]
  )

  // 根据路径查找文件夹
  const findFolderById = useCallback(
    (folderId: string): Folder | null => {
      const searchInFolders = (folderList: Folder[]): Folder | null => {
        for (const folder of folderList) {
          if (folder.id === folderId) {
            return folder
          }
          if (folder.children) {
            const found = searchInFolders(folder.children)
            if (found) return found
          }
        }
        return null
      }

      return searchInFolders(folders)
    },
    [folders]
  )

  // 根据ID查找标签
  const findTagById = useCallback(
    (tagId: string): Tag | null => {
      return tags.find((tag) => tag.id === tagId) || null
    },
    [tags]
  )

  // 获取选中项的详细信息
  const selectedFolderInfo = useMemo(() => {
    return selectedFolder ? findFolderById(selectedFolder) : null
  }, [selectedFolder, findFolderById])

  const selectedTagInfo = useMemo(() => {
    return selectedTag ? findTagById(selectedTag) : null
  }, [selectedTag, findTagById])

  // 添加新标签（占位符功能）
  const addNewTag = useCallback(() => {
    console.log('添加新标签功能')
    // 将来可以实现添加标签的逻辑
  }, [])

  // 获取当前选择的类型和名称
  const getCurrentSelection = useCallback(() => {
    if (selectedFolderInfo) {
      return {
        type: 'folder' as const,
        id: selectedFolder!,
        name: selectedFolderInfo.name,
        path: selectedFolderInfo.path,
      }
    }

    if (selectedTagInfo) {
      return {
        type: 'tag' as const,
        id: selectedTag!,
        name: selectedTagInfo.name,
        count: selectedTagInfo.count,
      }
    }

    return {
      type: 'all' as const,
      id: 'all',
      name: '所有媒体',
    }
  }, [selectedFolderInfo, selectedTagInfo, selectedFolder, selectedTag])

  return {
    // 数据
    folders,
    tags,

    // 状态
    sidebar,
    selectedFolder,
    selectedTag,
    selectedFolderInfo,
    selectedTagInfo,
    expandedFolders,

    // 选择操作
    selectFolder,
    selectTag,
    selectAllMedia,
    clearSelection,

    // 文件夹操作
    toggleFolderExpanded,
    isFolderExpanded,

    // 标签操作
    addNewTag,

    // 查询方法
    findFolderById,
    findTagById,
    getCurrentSelection,

    // 便捷属性
    hasSelection: selectedFolder !== null || selectedTag !== null,
    selectionType: selectedFolder ? 'folder' : selectedTag ? 'tag' : 'all',
  }
}
