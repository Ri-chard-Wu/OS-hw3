ssh_address=git@github.com:Ri-chard-Wu/OS-hw3.git

echo "*.o" > .gitignore

git init
git add *
git commit -m "first commit"
git remote add origin $ssh_address
git push -u origin master
