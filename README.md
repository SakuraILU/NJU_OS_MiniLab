# OSlab_NJU

这是南京大学 <a href="https://jyywiki.cn/OS/2022">2022 年的 OSLab</a>。该实验没有为校外同学提供本地测试，所以目前主要完成了五个 minilab 实验。五个 minilab 实验设计得很好，包括了文件读写、进程创建和交流、协程、动态链接、文件系统(<a href="https://jyywiki.cn/pages/OS/manuals/MSFAT-spec.pdf">Fat32</a>)、RTFM、 C 语言的一些小 tricky…

1. <a href="https://jyywiki.cn/OS/2022/labs/M1">pstree</a>: linux 的命令行工具 pstree 的基础版
2. <a href="https://jyywiki.cn/OS/2022/labs/M2">libco</a>: 一个精巧的协程库。完成后可结合<a href="https://www.bilibili.com/video/BV1cS4y1r7gw/?spm_id_from=333.788&vd_source=1a02f96a02d1fcf42776d7bde7447dd4">第七次课程</a>中间的 go 语言部分一起食用
3. <a href="https://jyywiki.cn/OS/2022/labs/M3">sperf</a>: 针对系统调用的性能检测工具
4. <a href="https://jyywiki.cn/OS/2022/labs/M4">C Real-Eval-Print-Loop(crepl)</a>: 一个 C 语言交互式 Shell，支持即时定义函数，而且能计算 C 表达式的数值。
5. <a href="https://jyywiki.cn/OS/2022/labs/M5">File Recovery (frecov)</a>: fat32 文件系统的 bitmap 图片恢复工具，能回复所有文件名和大概 50%的 bitmap 图，测试用的参考镜像文件见实验手册的 3.2 节。
