<script setup lang="ts">
import { useToast } from '@/composables/useToast'
import { Button } from '@/components/ui/button'

const { toast } = useToast()

// 基础示例
const showBasicToast = () => {
  toast('这是一条基础通知')
}

const showSuccessToast = () => {
  toast.success('操作成功！')
}

const showErrorToast = () => {
  toast.error('操作失败', {
    description: '请检查网络连接后重试',
  })
}

const showWarningToast = () => {
  toast.warning('警告：磁盘空间不足')
}

const showInfoToast = () => {
  toast.info('提示：新版本可用')
}

// 带描述的 Toast
const showWithDescription = () => {
  toast.success('文件上传成功', {
    description: '已成功上传 3 个文件到服务器',
  })
}

// 带操作按钮
const showWithAction = () => {
  toast('文件已删除', {
    action: {
      label: '撤销',
      onClick: () => {
        toast.info('已撤销删除操作')
      },
    },
  })
}

// 自定义时长
const showLongDuration = () => {
  toast.info('这条消息会显示 10 秒', {
    duration: 10000,
  })
}

// Loading 状态
const showLoadingToast = () => {
  const loadingToast = toast.loading('正在处理...')
  
  setTimeout(() => {
    toast.dismiss(loadingToast)
    toast.success('处理完成！')
  }, 3000)
}

// Promise Toast
const showPromiseToast = async () => {
  const simulateAsync = () => 
    new Promise((resolve, reject) => {
      setTimeout(() => {
        Math.random() > 0.5 ? resolve('Success') : reject(new Error('Failed'))
      }, 2000)
    })

  toast.promise(simulateAsync(), {
    loading: '正在保存设置...',
    success: '设置已保存',
    error: (err) => `保存失败: ${err.message}`,
  })
}

// 多个同时显示
const showMultipleToasts = () => {
  toast.success('第一条消息')
  setTimeout(() => toast.info('第二条消息'), 500)
  setTimeout(() => toast.warning('第三条消息'), 1000)
}
</script>

<template>
  <div class="space-y-8">
    <div>
      <h2 class="text-2xl font-bold mb-4">Toast 通知组件演示</h2>
      <p class="text-muted-foreground mb-6">
        演示 vue-sonner toast 组件的各种用法
      </p>
    </div>

    <!-- 基础类型 -->
    <section>
      <h3 class="text-lg font-semibold mb-3">基础类型</h3>
      <div class="flex flex-wrap gap-2">
        <Button @click="showBasicToast" variant="outline">
          基础 Toast
        </Button>
        <Button @click="showSuccessToast" variant="outline">
          成功
        </Button>
        <Button @click="showErrorToast" variant="outline">
          错误
        </Button>
        <Button @click="showWarningToast" variant="outline">
          警告
        </Button>
        <Button @click="showInfoToast" variant="outline">
          信息
        </Button>
      </div>
    </section>

    <!-- 高级功能 -->
    <section>
      <h3 class="text-lg font-semibold mb-3">高级功能</h3>
      <div class="flex flex-wrap gap-2">
        <Button @click="showWithDescription" variant="outline">
          带描述
        </Button>
        <Button @click="showWithAction" variant="outline">
          带操作按钮
        </Button>
        <Button @click="showLongDuration" variant="outline">
          长时间显示
        </Button>
        <Button @click="showLoadingToast" variant="outline">
          Loading 状态
        </Button>
        <Button @click="showPromiseToast" variant="outline">
          Promise Toast
        </Button>
        <Button @click="showMultipleToasts" variant="outline">
          多个通知
        </Button>
      </div>
    </section>

    <!-- 代码示例 -->
    <section>
      <h3 class="text-lg font-semibold mb-3">使用示例</h3>
      <div class="space-y-4 text-sm">
        <div class="rounded-lg border p-4 bg-muted/50">
          <p class="font-mono text-xs">
            <span class="text-muted-foreground">// 基础用法</span><br />
            const { toast } = useToast()<br />
            toast.success('操作成功')
          </p>
        </div>
        
        <div class="rounded-lg border p-4 bg-muted/50">
          <p class="font-mono text-xs">
            <span class="text-muted-foreground">// 带描述</span><br />
            toast.error('操作失败', {<br />
            &nbsp;&nbsp;description: '网络连接超时'<br />
            })
          </p>
        </div>

        <div class="rounded-lg border p-4 bg-muted/50">
          <p class="font-mono text-xs">
            <span class="text-muted-foreground">// 带操作按钮</span><br />
            toast('确认删除？', {<br />
            &nbsp;&nbsp;action: {<br />
            &nbsp;&nbsp;&nbsp;&nbsp;label: '撤销',<br />
            &nbsp;&nbsp;&nbsp;&nbsp;onClick: () => console.log('Undo')<br />
            &nbsp;&nbsp;}<br />
            })
          </p>
        </div>

        <div class="rounded-lg border p-4 bg-muted/50">
          <p class="font-mono text-xs">
            <span class="text-muted-foreground">// Promise Toast</span><br />
            toast.promise(saveSettings(), {<br />
            &nbsp;&nbsp;loading: '保存中...',<br />
            &nbsp;&nbsp;success: '保存成功',<br />
            &nbsp;&nbsp;error: '保存失败'<br />
            })
          </p>
        </div>
      </div>
    </section>
  </div>
</template>
