import { useWebSettingsStore } from '@/lib/web-settings'

/**
 * 前端配置系统测试组件
 * 用于验证 web-settings 功能是否正常工作
 */
export function WebSettingsTest() {
  const {
    settings,
    error,
    updateBackgroundSettings,
    selectAndSetBackgroundImage,
    removeBackgroundImage,
    resetToDefault,
  } = useWebSettingsStore()

  // 注意：WebSettings 的初始化已在 AppLayout 中处理，这里不需要重复初始化

  if (error) {
    return <div>❌ 错误: {error}</div>
  }

  const handleOpacityChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const opacity = parseFloat(event.target.value)
    updateBackgroundSettings({ opacity })
  }

  const handleSelectImage = async () => {
    try {
      await selectAndSetBackgroundImage()
    } catch (error) {
      console.error('选择图片失败:', error)
    }
  }

  const handleRemoveBackground = async () => {
    try {
      await removeBackgroundImage()
    } catch (error) {
      console.error('移除背景失败:', error)
    }
  }

  const handleReset = async () => {
    try {
      await resetToDefault()
    } catch (error) {
      console.error('重置失败:', error)
    }
  }

  return (
    <div style={{ padding: '20px', border: '1px solid #ccc', margin: '20px' }}>
      <h3>🧪 前端配置系统测试</h3>

      <div style={{ marginBottom: '20px' }}>
        <h4>📋 当前配置状态</h4>
        <p>
          <strong>版本:</strong> {settings.version}
        </p>
        <p>
          <strong>背景类型:</strong> {settings.ui.background.type}
        </p>
        <p>
          <strong>图片路径:</strong> {settings.ui.background.imagePath || '无'}
        </p>
        <p>
          <strong>透明度:</strong> {settings.ui.background.opacity}
        </p>
        <p>
          <strong>创建时间:</strong> {new Date(settings.createdAt).toLocaleString()}
        </p>
        <p>
          <strong>更新时间:</strong> {new Date(settings.updatedAt).toLocaleString()}
        </p>
      </div>

      <div style={{ marginBottom: '20px' }}>
        <h4>🎛️ 背景设置控制</h4>

        <div style={{ marginBottom: '10px' }}>
          <label>
            <strong>透明度: </strong>
            <input
              type='range'
              min='0'
              max='1'
              step='0.1'
              value={settings.ui.background.opacity}
              onChange={handleOpacityChange}
              style={{ marginLeft: '10px' }}
            />
            <span style={{ marginLeft: '10px' }}>
              {(settings.ui.background.opacity * 100).toFixed(0)}%
            </span>
          </label>
        </div>

        <div style={{ marginBottom: '10px' }}>
          <button onClick={handleSelectImage} style={{ marginRight: '10px', padding: '5px 10px' }}>
            🖼️ 选择背景图片
          </button>

          <button
            onClick={handleRemoveBackground}
            style={{ marginRight: '10px', padding: '5px 10px' }}
            disabled={settings.ui.background.type === 'none'}
          >
            🚫 移除背景
          </button>

          <button onClick={handleReset} style={{ padding: '5px 10px' }}>
            🔄 重置为默认
          </button>
        </div>
      </div>

      <div
        style={{
          marginTop: '20px',
          padding: '10px',
          backgroundColor: '#f5f5f5',
          borderRadius: '4px',
        }}
      >
        <h4>📊 配置JSON预览</h4>
        <pre
          style={{
            fontSize: '12px',
            overflow: 'auto',
            backgroundColor: '#fff',
            padding: '10px',
            border: '1px solid #ddd',
            borderRadius: '4px',
          }}
        >
          {JSON.stringify(settings, null, 2)}
        </pre>
      </div>
    </div>
  )
}
