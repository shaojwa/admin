# git_man

###### v0.1 on 20131219 / v0.2 on 20141125

## ����git

    git config user.email "shaojwa@gmail.com"
    git config user.name "shaojwa"

## ��������ֿ�
### ����1
��github��վ�ϴ�������ֿ�

### ����2

## ��ʼ��
ʹ��ǰ�ȳ�ʼ�����زֿ�   
����1�����س�ʼ������ʱ����������ֿ⣩

    cd <dir>
    git init
����2����Զ�ֿ̲��ʼ����ֱ�ӹ���ĳ������ֿ⣩

    cd <dir>
    git clone https://github.com/shaojwa/leetcode/git

## �޸ĺ�ĳ���
�����������е������޸ģ�

    git reset --hard HEAD
������������ָ���ļ��ĵ��޸ģ�
    
    
## ����ļ������ػ���(index-tree)
����1�����ĳ��Ŀ¼�µ������ļ������ػ��� 

    git add . 
    git add -u .
    
����2�����ָ���ļ������ػ���

    git add -p <file>

## ������鿴״̬
    git status

##ȡ����ӵ�����
����1��ȡ��ĳ���ļ������

    git rm --cached <file>
   
����2��ȡ�������ļ������


## �Ѹ���ͬ�������زֿ�(head-tree)
ͬ����Զ�̷�����ǰ����ͬ�������ط�����

    git commit -m "some message"

## ���޸�ͬ����Զ�ֿ̲�

    git push  https://github.com/shaojwa/leetcode.git maser
    
���������������ʾ�������û���������:

    Username for 'https://github.com':
    Password for 'https://shaojwa@github.com':
    
������ȷ���û���������֮����ʾ�ύ�ɹ�.��������վ��ȷ�ϡ�

    
    
##��֧����

Զ������µķ�֧
 
	git remote add doc https://github.com/shaojwa/doc.git

[it switch](sw.jpg)

    git commit -m "`date`"
