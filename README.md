# PCI_driver_template
to use driver:
- sudo make
- sudo insmod devboard.ko irq_en=false
- sudo rmmod devboard.ko

Additional: 
- modinfo devboard.ko
- mknod /dev/test c MAJ_NUMBER 0
  - head -n 1 /dev/test  /* to read GPIO_I state */
  - echo (number 0-7) > /dev/test  /* to write GPIO_O state */
