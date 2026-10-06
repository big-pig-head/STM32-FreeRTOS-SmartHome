#ifndef __SOUND_H__
#define __SOUND_H__

void Music_Play_Start(void);   /* 非阻塞播放：开始/恢复播放，初始化TIM2并播第一个音符 */
void Music_Play_Process(void); /* 非阻塞播放：驱动音符状态机（每10ms调用一次，不阻塞） */


#endif
