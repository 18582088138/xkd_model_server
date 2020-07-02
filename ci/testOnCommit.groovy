pipeline {
    agent {
      label 'ovmscheck'
    }
    stages {
        stage('Configure') {
          steps {
            script {
              checkout scm
              shortCommit = sh(returnStdout: true, script: "git log -n 1 --pretty=format:'%h'").trim()
              echo shortCommit
            }
          }
        }
        stage('style check') {
            steps {
                sh 'make style'
            }
        }

        stage('docker build') {
            steps {
                sh 'make docker_build'
            }
        }

        stage('latency test') {
            steps {
                lock("commit-pipeline-${env.NODE_NAME}-latency-test") {
                    sh 'make test_perf'
                }
            }
        }

        stage('throughput test') {
            steps {
                lock("commit-pipeline-${env.NODE_NAME}-throughput-test") {
                    sh 'make test_throughput'
                }
            }
        }

        stage("Run smoke and regression tests on commit") {
          steps {
              sh """
              env
              """
              echo shortCommit
              build job: "ovmsc/util-common/ovmsc-test-on-commit", parameters: [[$class: 'StringParameterValue', name: 'OVMSCCOMMIT', value: shortCommit]]
          }    
        }
    }
}
