<script setup lang="ts">
import { ref, computed } from 'vue'
import { NModal, NForm, NFormItem, NInput, NSpace, NButton } from 'naive-ui'
import type { Album } from '@/types/album'

const props = defineProps<{
  show: boolean
  album: Album
}>()

const emit = defineEmits<{
  (e: 'update:show', value: boolean): void
  (e: 'save', album: { name: string; description?: string }): void
}>()

const formRef = ref()
const formValue = ref({
  name: props.album.name,
  description: props.album.description || ''
})

// 使用计算属性处理show属性
const dialogVisible = computed({
  get: () => props.show,
  set: (value) => emit('update:show', value)
})

// 表单验证规则
const rules = {
  name: {
    required: true,
    message: '请输入相册名称',
    trigger: ['blur', 'input']
  }
}

// 保存修改
async function handleSave() {
  try {
    await formRef.value?.validate()
    emit('save', {
      name: formValue.value.name,
      description: formValue.value.description
    })
    emit('update:show', false)
  } catch (err) {
    // 表单验证失败
  }
}

// 关闭对话框
function handleClose() {
  emit('update:show', false)
}
</script>

<template>
  <n-modal
    v-model:show="dialogVisible"
    preset="card"
    title="编辑相册"
    :mask-closable="false"
    @close="handleClose"
  >
    <n-form
      ref="formRef"
      :model="formValue"
      :rules="rules"
      label-placement="left"
      label-width="80"
      require-mark-placement="right-hanging"
    >
      <n-form-item label="名称" path="name">
        <n-input
          v-model:value="formValue.name"
          placeholder="请输入相册名称"
        />
      </n-form-item>
      <n-form-item label="描述" path="description">
        <n-input
          v-model:value="formValue.description"
          type="textarea"
          placeholder="请输入相册描述"
        />
      </n-form-item>
    </n-form>
    
    <template #footer>
      <n-space justify="end">
        <n-button @click="handleClose">取消</n-button>
        <n-button type="primary" @click="handleSave">保存</n-button>
      </n-space>
    </template>
  </n-modal>
</template> 