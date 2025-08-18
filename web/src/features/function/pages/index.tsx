import { useEffect } from 'react'
import { FunctionContent } from '../components/function-content'
// 模拟store hook，实际应该从store中导入
const useFunctionStore = () => {
  return {
    clearError: () => {},
  }
}

export function FunctionPage() {
  const { clearError } = useFunctionStore()

  // 清除之前的错误状态
  useEffect(() => {
    clearError()
  }, [clearError])

  return (
    <div>
      {/* 功能设置内容区域 */}
      <FunctionContent />
    </div>
  )
}

export default FunctionPage
