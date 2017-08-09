This program allows to mount virtual disk file and use it thereafter as ordinary disk.
It runs on all 32-bit versions of Windows, starting with Windows XP.

virtual disk file types supported:
plain (see below)
vmware (vmdk files from vmware virtual machines)

The file must exist. The program doesn't provide for creating virtual disks.
Use:

	diskctl add <num> <type> <file>
	diskctl remove <num>


Simple tutorial:

1. create file (that will serve as disk):
	fsutil file createnew c:\testdir\testfile.dat 10000000000
(this creates file c:\testdir\testfile.dat with size 10GB).
2. mount this file as disk:
	diskctl add 1 plain c:\testdir\testfile.dat
where:
	add is command to mount file;
	1 is arbitrary 32-bit number; serves as disk name to distinguish different disks
	plain is disk type. It means that file contains just disk sectors and nothing more 
	(headers etc).
	last argument is file name.

As a result you will have this file attached to your system as disk. As file is empty, 
the disk will be empty too - without even partition table. If you run Disk Administrator 
on your computer (My computer->Manage->Storage->Disk Management), you will be prompted to 
initialize the disk. After that you can do whatever you want with it: create partitions, 
format them etc.

After you are done with it, dismount:
	diskctl remove 1
where:
	remove is command to dismount file;
	1 is the number from add command.

To mount vmware disk:
	diskctl add 2 vmware "Windows Server 2003 Standard Edition.vmdk"
(in order for it to work, the Virtual Disk Development Kit must be installed, and VMWare plugin from this package).

You can mount as many disks simultaneously as you want.

You can see latest news at http://www.vddeveloper.ru/virtualdisk.aspx
and leave all your questions and remarks about this program there in guestbook.
